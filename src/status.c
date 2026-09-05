#include "status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXED_MANGLE_COUNT 8
#define PROTO_COUNT 2
#define STATE_COUNT 4

enum {
    PROTO_TCP = 0,
    PROTO_UDP = 1
};

enum {
    STATE_OK = 0,
    STATE_TEST = 1,
    STATE_WATCH = 2,
    STATE_COOLDOWN = 3
};

typedef struct {
    char **names;
    size_t count;
    size_t cap;
} bucket_vec_t;

typedef struct {
    unsigned scripts;
    unsigned schedulers;

    unsigned fixed_mangle;
    unsigned fixed_duplicates;
    unsigned lazy_mangle;
    unsigned unknown_auto_mangle;
    int fixed_seen[FIXED_MANGLE_COUNT];

    unsigned state_entries[PROTO_COUNT][STATE_COUNT];
    unsigned state_buckets[PROTO_COUNT][STATE_COUNT];
    unsigned legacy_entries;

    bucket_vec_t buckets;
} status_ctx_t;

static const char *fixed_mangle_comments[FIXED_MANGLE_COUNT] = {
    "AUTO-AWG: L2 mark confirmed",
    "AUTO-AWG: L1 mark test",
    "AUTO-AWG: L2 mark confirmed UDP",
    "AUTO-AWG: L1 mark test UDP",
    "AUTO-AWG: L2 route confirmed",
    "AUTO-AWG: L1 route test",
    "AUTO-AWG: router DNS UDP via tunnel",
    "AUTO-AWG: router DNS TCP via tunnel"
};

typedef struct {
    const char *prefix;
    int proto;
    int state;
} port_prefix_t;

static const port_prefix_t port_prefixes[] = {
    {"auto_awg_ok_tcp_",       PROTO_TCP, STATE_OK},
    {"auto_awg_test_tcp_",     PROTO_TCP, STATE_TEST},
    {"auto_awg_watch_tcp_",    PROTO_TCP, STATE_WATCH},
    {"auto_awg_cooldown_tcp_", PROTO_TCP, STATE_COOLDOWN},
    {"auto_awg_ok_udp_",       PROTO_UDP, STATE_OK},
    {"auto_awg_test_udp_",     PROTO_UDP, STATE_TEST},
    {"auto_awg_watch_udp_",    PROTO_UDP, STATE_WATCH},
    {"auto_awg_cooldown_udp_", PROTO_UDP, STATE_COOLDOWN}
};

static const char *legacy_lists[] = {
    "auto_awg_ok_tcp",
    "auto_awg_test_tcp",
    "auto_awg_watch_tcp",
    "auto_awg_cooldown_tcp",
    "auto_awg_ok_udp",
    "auto_awg_test_udp",
    "auto_awg_watch_udp",
    "auto_awg_cooldown_udp"
};

static int has_prefix(
    const char *value,
    const char *prefix
) {
    return
        value &&
        prefix &&
        strncmp(
            value,
            prefix,
            strlen(prefix)
        ) == 0;
}

static char *dup_string(
    const char *s
) {
    if (!s) return NULL;

    size_t n = strlen(s) + 1;
    char *out = malloc(n);

    if (!out) return NULL;

    memcpy(out, s, n);
    return out;
}

static void bucket_vec_free(
    bucket_vec_t *v
) {
    if (!v) return;

    for (size_t i = 0; i < v->count; ++i) {
        free(v->names[i]);
    }

    free(v->names);

    v->names = NULL;
    v->count = 0;
    v->cap = 0;
}

static int bucket_vec_add_unique(
    bucket_vec_t *v,
    const char *name
) {
    if (!v || !name || !*name) return -1;

    for (size_t i = 0; i < v->count; ++i) {
        if (strcmp(v->names[i], name) == 0) {
            return 0;
        }
    }

    if (v->count == v->cap) {
        size_t new_cap =
            v->cap
                ? v->cap * 2
                : 16;

        char **new_names = realloc(
            v->names,
            new_cap * sizeof(*new_names)
        );

        if (!new_names) return -1;

        v->names = new_names;
        v->cap = new_cap;
    }

    v->names[v->count] =
        dup_string(name);

    if (!v->names[v->count]) {
        return -1;
    }

    v->count++;
    return 1;
}

static int valid_port_suffix(
    const char *s
) {
    if (!s || !*s) return 0;

    unsigned long port = 0;

    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return 0;
        }

        port =
            port * 10UL +
            (unsigned long)(*p - '0');

        if (port > 65535UL) {
            return 0;
        }
    }

    return 1;
}

static int parse_port_list(
    const char *list,
    int *proto,
    int *state
) {
    if (!list || !proto || !state) {
        return 0;
    }

    for (
        size_t i = 0;
        i < sizeof(port_prefixes) / sizeof(port_prefixes[0]);
        ++i
    ) {
        size_t n =
            strlen(port_prefixes[i].prefix);

        if (
            strncmp(
                list,
                port_prefixes[i].prefix,
                n
            ) == 0 &&
            valid_port_suffix(list + n)
        ) {
            *proto = port_prefixes[i].proto;
            *state = port_prefixes[i].state;
            return 1;
        }
    }

    return 0;
}

static int is_legacy_list(
    const char *list
) {
    if (!list) return 0;

    for (
        size_t i = 0;
        i < sizeof(legacy_lists) / sizeof(legacy_lists[0]);
        ++i
    ) {
        if (
            strcmp(
                list,
                legacy_lists[i]
            ) == 0
        ) {
            return 1;
        }
    }

    return 0;
}

static int fixed_mangle_index(
    const char *comment
) {
    if (!comment) return -1;

    for (size_t i = 0; i < FIXED_MANGLE_COUNT; ++i) {
        if (
            strcmp(
                comment,
                fixed_mangle_comments[i]
            ) == 0
        ) {
            return (int)i;
        }
    }

    return -1;
}

static int script_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    status_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    const char *name =
        ros_get_attr(s, "name");

    const char *disabled =
        ros_get_attr(s, "disabled");

    if (!name) return 0;

    if (
        strcmp(name, "auto-awg-health") == 0 ||
        strcmp(name, "auto-awg-fast") == 0 ||
        strcmp(name, "auto-awg-detect") == 0 ||
        strcmp(name, "auto-awg-judge") == 0
    ) {
        printf(
            "  %-18s disabled=%s\n",
            name,
            disabled
                ? disabled
                : "false"
        );

        ctx->scripts++;
    }

    return 0;
}

static int scheduler_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    status_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    const char *name =
        ros_get_attr(s, "name");

    const char *interval =
        ros_get_attr(s, "interval");

    const char *disabled =
        ros_get_attr(s, "disabled");

    const char *run_count =
        ros_get_attr(s, "run-count");

    if (
        !name ||
        strncmp(
            name,
            "auto-awg-",
            9
        ) != 0
    ) {
        return 0;
    }

    printf(
        "  %-18s interval=%-8s disabled=%-5s runs=%s\n",
        name,
        interval ? interval : "?",
        disabled ? disabled : "false",
        run_count ? run_count : "?"
    );

    ctx->schedulers++;
    return 0;
}

static int mangle_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    status_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    const char *comment =
        ros_get_attr(s, "comment");

    if (
        !comment ||
        !has_prefix(
            comment,
            "AUTO-AWG:"
        )
    ) {
        return 0;
    }

    /*
     * Lazy port rules can grow with observed protocol/port buckets.
     * Count them, but do not flood normal status output with each rule.
     */
    if (
        has_prefix(
            comment,
            "AUTO-AWG: P "
        )
    ) {
        ctx->lazy_mangle++;
        return 0;
    }

    int idx =
        fixed_mangle_index(comment);

    if (idx < 0) {
        ctx->unknown_auto_mangle++;

        printf(
            "  UNKNOWN %-38s disabled=%s\n",
            comment,
            ros_get_attr(s, "disabled")
                ? ros_get_attr(s, "disabled")
                : "false"
        );

        return 0;
    }

    if (ctx->fixed_seen[idx]) {
        ctx->fixed_duplicates++;
    } else {
        ctx->fixed_seen[idx] = 1;
        ctx->fixed_mangle++;
    }

    const char *action =
        ros_get_attr(s, "action");

    const char *proto =
        ros_get_attr(s, "protocol");

    const char *list =
        ros_get_attr(
            s,
            "dst-address-list"
        );

    const char *disabled =
        ros_get_attr(s, "disabled");

    printf(
        "  %-38s action=%-15s proto=%-4s list=%-22s disabled=%s\n",
        comment,
        action ? action : "?",
        proto ? proto : "-",
        list ? list : "-",
        disabled ? disabled : "false"
    );

    return 0;
}

static int addrlist_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    status_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    const char *list =
        ros_get_attr(s, "list");

    if (!list) return 0;

    int proto = -1;
    int state = -1;

    if (
        parse_port_list(
            list,
            &proto,
            &state
        )
    ) {
        ctx->state_entries[proto][state]++;

        int added =
            bucket_vec_add_unique(
                &ctx->buckets,
                list
            );

        if (added < 0) {
            return -1;
        }

        if (added > 0) {
            ctx->state_buckets[proto][state]++;
        }

        return 0;
    }

    if (is_legacy_list(list)) {
        ctx->legacy_entries++;
    }

    return 0;
}

static int iface_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    const app_config_t *cfg = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    const char *name =
        ros_get_attr(s, "name");

    if (
        !name ||
        !cfg->egress_interface ||
        strcmp(
            name,
            cfg->egress_interface
        ) != 0
    ) {
        return 0;
    }

    printf(
        "Egress: %s running=%s disabled=%s type=%s\n",
        name,
        ros_get_attr(s, "running")
            ? ros_get_attr(s, "running")
            : "?",
        ros_get_attr(s, "disabled")
            ? ros_get_attr(s, "disabled")
            : "?",
        ros_get_attr(s, "type")
            ? ros_get_attr(s, "type")
            : "?"
    );

    return 0;
}

static void print_protocol_state(
    const char *name,
    const status_ctx_t *ctx,
    int proto
) {
    printf(
        "  %s buckets: ok=%u test=%u watch=%u cooldown=%u\n",
        name,
        ctx->state_buckets[proto][STATE_OK],
        ctx->state_buckets[proto][STATE_TEST],
        ctx->state_buckets[proto][STATE_WATCH],
        ctx->state_buckets[proto][STATE_COOLDOWN]
    );

    printf(
        "  %s entries: ok=%u test=%u watch=%u cooldown=%u\n",
        name,
        ctx->state_entries[proto][STATE_OK],
        ctx->state_entries[proto][STATE_TEST],
        ctx->state_entries[proto][STATE_WATCH],
        ctx->state_entries[proto][STATE_COOLDOWN]
    );
}

int status_run(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    status_ctx_t ctx;

    memset(
        &ctx,
        0,
        sizeof(ctx)
    );

    printf("=== SUSANIN STATUS ===\n");

    printf(
        "Target: %s %s\n",
        config_target_mode_name(
            cfg->target_mode
        ),
        cfg->target_value
            ? cfg->target_value
            : "<not selected>"
    );

    printf(
        "Routing table: %s\n",
        cfg->routing_table
            ? cfg->routing_table
            : "<not selected>"
    );

    if (cfg->egress_interface) {
        const char *ifs[] = {
            "/interface/print",
            "=.proplist=name,type,running,disabled"
        };

        if (
            ros_command(
                ros,
                ifs,
                2,
                iface_cb,
                (void *)cfg
            ) < 0
        ) {
            return -1;
        }
    } else if (
        cfg->target_mode ==
            SUSANIN_TARGET_ROUTING_TABLE
    ) {
        printf(
            "Egress: <table-native / not required>\n"
        );
    }

    printf("\nScripts:\n");

    const char *scripts[] = {
        "/system/script/print",
        "=.proplist=name,disabled"
    };

    if (
        ros_command(
            ros,
            scripts,
            2,
            script_cb,
            &ctx
        ) < 0
    ) {
        bucket_vec_free(&ctx.buckets);
        return -1;
    }

    printf("\nSchedulers:\n");

    const char *sched[] = {
        "/system/scheduler/print",
        "=.proplist=name,interval,disabled,run-count"
    };

    if (
        ros_command(
            ros,
            sched,
            2,
            scheduler_cb,
            &ctx
        ) < 0
    ) {
        bucket_vec_free(&ctx.buckets);
        return -1;
    }

    printf("\nFixed mangle rules:\n");

    const char *mangle[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=comment,action,protocol,dst-address-list,disabled"
    };

    if (
        ros_command(
            ros,
            mangle,
            2,
            mangle_cb,
            &ctx
        ) < 0
    ) {
        bucket_vec_free(&ctx.buckets);
        return -1;
    }

    const char *alist[] = {
        "/ip/firewall/address-list/print",
        "=.proplist=list"
    };

    if (
        ros_command(
            ros,
            alist,
            2,
            addrlist_cb,
            &ctx
        ) < 0
    ) {
        bucket_vec_free(&ctx.buckets);
        return -1;
    }

    printf("\nPort-aware state cache:\n");

    print_protocol_state(
        "TCP",
        &ctx,
        PROTO_TCP
    );

    print_protocol_state(
        "UDP",
        &ctx,
        PROTO_UDP
    );

    printf(
        "  legacy IP-only entries=%u%s\n",
        ctx.legacy_entries,
        ctx.legacy_entries
            ? "  [MIGRATION RESIDUE]"
            : ""
    );

    printf("\nDynamic dataplane:\n");

    printf(
        "  lazy port mangle rules=%u\n",
        ctx.lazy_mangle
    );

    printf(
        "  unknown AUTO-AWG rules=%u\n",
        ctx.unknown_auto_mangle
    );

    printf(
        "\nSummary: scripts=%u/4 schedulers=%u/4 "
        "fixed-mangle=%u/8 fixed-duplicates=%u lazy-mangle=%u\n",
        ctx.scripts,
        ctx.schedulers,
        ctx.fixed_mangle,
        ctx.fixed_duplicates,
        ctx.lazy_mangle
    );

    if (
        ctx.scripts == 4 &&
        ctx.schedulers == 4 &&
        ctx.fixed_mangle == FIXED_MANGLE_COUNT &&
        ctx.fixed_duplicates == 0
    ) {
        printf(
            "Installation state: detected\n"
        );
    } else {
        printf(
            "Installation state: incomplete or inconsistent\n"
        );
    }

    if (ctx.legacy_entries) {
        printf(
            "Adaptive migration state: legacy IP-only residue detected\n"
        );
    } else {
        printf(
            "Adaptive migration state: clean\n"
        );
    }

    bucket_vec_free(
        &ctx.buckets
    );

    return 0;
}
