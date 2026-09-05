#define _POSIX_C_SOURCE 200809L

#include "state_cleanup.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} id_vec_t;

typedef int (*value_match_fn)(
    const char *value,
    const char *pattern
);

typedef struct {
    const char *attr;
    const char *pattern;
    value_match_fn match;
    id_vec_t *ids;
} collect_ctx_t;

static void id_vec_free(
    id_vec_t *v
) {
    if (!v) return;

    for (size_t i = 0; i < v->count; ++i) {
        free(v->items[i]);
    }

    free(v->items);
    memset(v, 0, sizeof(*v));
}

static int id_vec_add(
    id_vec_t *v,
    const char *id
) {
    if (!v || !id || !*id) return -1;

    if (v->count == v->cap) {
        size_t new_cap =
            v->cap
                ? v->cap * 2
                : 16;

        char **new_items = realloc(
            v->items,
            new_cap * sizeof(*new_items)
        );

        if (!new_items) return -1;

        v->items = new_items;
        v->cap = new_cap;
    }

    v->items[v->count] = strdup(id);

    if (!v->items[v->count]) return -1;

    v->count++;
    return 0;
}

static int match_exact(
    const char *value,
    const char *pattern
) {
    return
        value &&
        pattern &&
        strcmp(value, pattern) == 0;
}

static int match_prefix(
    const char *value,
    const char *pattern
) {
    return
        value &&
        pattern &&
        strncmp(
            value,
            pattern,
            strlen(pattern)
        ) == 0;
}

static int port_suffix_valid(
    const char *s
) {
    if (!s || !*s) return 0;

    unsigned long port = 0;

    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;

        port =
            port * 10UL +
            (unsigned long)(*p - '0');

        if (port > 65535UL) return 0;
    }

    return 1;
}

static int match_dev2_port_list(
    const char *value,
    const char *pattern
) {
    (void)pattern;

    if (!value) return 0;

    static const char *prefixes[] = {
        "auto_awg_watch_tcp_",
        "auto_awg_test_tcp_",
        "auto_awg_ok_tcp_",
        "auto_awg_cooldown_tcp_",
        "auto_awg_watch_udp_",
        "auto_awg_test_udp_",
        "auto_awg_ok_udp_",
        "auto_awg_cooldown_udp_"
    };

    for (
        size_t i = 0;
        i < sizeof(prefixes) / sizeof(prefixes[0]);
        ++i
    ) {
        size_t n = strlen(prefixes[i]);

        if (
            strncmp(
                value,
                prefixes[i],
                n
            ) == 0 &&
            port_suffix_valid(value + n)
        ) {
            return 1;
        }
    }

    return 0;
}

static int collect_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    collect_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *id =
        ros_get_attr(s, ".id");

    const char *value =
        ros_get_attr(s, ctx->attr);

    if (
        !id ||
        !*id ||
        !ctx->match(
            value,
            ctx->pattern
        )
    ) {
        return 0;
    }

    return id_vec_add(
        ctx->ids,
        id
    );
}

static int collect_ids(
    ros_client_t *ros,
    const char *print_cmd,
    const char *proplist,
    const char *query,
    const char *attr,
    value_match_fn match,
    const char *pattern,
    id_vec_t *out
) {
    if (
        !ros ||
        !print_cmd ||
        !proplist ||
        !attr ||
        !match ||
        !out
    ) {
        return -1;
    }

    memset(
        out,
        0,
        sizeof(*out)
    );

    collect_ctx_t ctx = {
        .attr = attr,
        .pattern = pattern,
        .match = match,
        .ids = out
    };

    const char *cmd[3];
    size_t n = 0;

    cmd[n++] = print_cmd;
    cmd[n++] = proplist;

    if (query && *query) {
        cmd[n++] = query;
    }

    if (
        ros_command(
            ros,
            cmd,
            n,
            collect_cb,
            &ctx
        ) < 0
    ) {
        id_vec_free(out);
        return -1;
    }

    return 0;
}

static void add_stat(
    unsigned *dst,
    size_t value
) {
    if (!dst) return;

    if (
        value >
        (size_t)(UINT_MAX - *dst)
    ) {
        *dst = UINT_MAX;
        return;
    }

    *dst += (unsigned)value;
}

static void remove_ids_best_effort(
    ros_client_t *ros,
    const char *remove_cmd,
    const id_vec_t *ids
) {
    if (
        !ros ||
        !remove_cmd ||
        !ids
    ) {
        return;
    }

    for (
        size_t i = 0;
        i < ids->count;
        ++i
    ) {
        char wid[96];

        snprintf(
            wid,
            sizeof(wid),
            "=.id=%s",
            ids->items[i]
        );

        const char *cmd[] = {
            remove_cmd,
            wid
        };

        /*
         * State entries and connections can expire/disappear between
         * print and remove. Ignore the individual race and verify the
         * requested post-condition with a fresh specialized query.
         */
        (void)ros_command(
            ros,
            cmd,
            2,
            NULL,
            NULL
        );
    }
}

static int cleanup_matching(
    ros_client_t *ros,
    const char *print_cmd,
    const char *remove_cmd,
    const char *proplist,
    const char *query,
    const char *attr,
    value_match_fn match,
    const char *pattern,
    unsigned *removed
) {
    id_vec_t before;

    if (
        collect_ids(
            ros,
            print_cmd,
            proplist,
            query,
            attr,
            match,
            pattern,
            &before
        ) < 0
    ) {
        return -1;
    }

    size_t initial = before.count;

    remove_ids_best_effort(
        ros,
        remove_cmd,
        &before
    );

    id_vec_free(&before);

    id_vec_t after;

    if (
        collect_ids(
            ros,
            print_cmd,
            proplist,
            query,
            attr,
            match,
            pattern,
            &after
        ) < 0
    ) {
        return -1;
    }

    int ok =
        after.count == 0;

    id_vec_free(&after);

    if (!ok) return -1;

    add_stat(
        removed,
        initial
    );

    return 0;
}

static int cleanup_exact_list(
    ros_client_t *ros,
    const char *list,
    unsigned *removed
) {
    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?list=%s",
        list
    );

    return cleanup_matching(
        ros,
        "/ip/firewall/address-list/print",
        "/ip/firewall/address-list/remove",
        "=.proplist=.id,list",
        query,
        "list",
        match_exact,
        list,
        removed
    );
}

static int cleanup_connection_mark(
    ros_client_t *ros,
    const char *mark,
    unsigned *removed
) {
    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?connection-mark=%s",
        mark
    );

    return cleanup_matching(
        ros,
        "/ip/firewall/connection/print",
        "/ip/firewall/connection/remove",
        "=.proplist=.id,connection-mark",
        query,
        "connection-mark",
        match_exact,
        mark,
        removed
    );
}

static int cleanup_port_lists(
    ros_client_t *ros,
    unsigned *removed
) {
    return cleanup_matching(
        ros,
        "/ip/firewall/address-list/print",
        "/ip/firewall/address-list/remove",
        "=.proplist=.id,list",
        NULL,
        "list",
        match_dev2_port_list,
        NULL,
        removed
    );
}

static int cleanup_lazy_rules(
    ros_client_t *ros,
    unsigned *removed
) {
    return cleanup_matching(
        ros,
        "/ip/firewall/mangle/print",
        "/ip/firewall/mangle/remove",
        "=.proplist=.id,comment",
        NULL,
        "comment",
        match_prefix,
        "AUTO-AWG: P ",
        removed
    );
}

static int cleanup_legacy_into(
    ros_client_t *ros,
    susanin_cleanup_stats_t *stats
) {
    static const char *legacy_lists[] = {
        "auto_awg_watch_tcp",
        "auto_awg_test_tcp",
        "auto_awg_ok_tcp",
        "auto_awg_cooldown_tcp",
        "auto_awg_watch_udp",
        "auto_awg_test_udp",
        "auto_awg_ok_udp",
        "auto_awg_cooldown_udp"
    };

    for (
        size_t i = 0;
        i < sizeof(legacy_lists) / sizeof(legacy_lists[0]);
        ++i
    ) {
        if (
            cleanup_exact_list(
                ros,
                legacy_lists[i],
                &stats->legacy_entries
            ) < 0
        ) {
            return -1;
        }
    }

    static const char *marks[] = {
        "auto-awg-test-conn",
        "auto-awg-ok-conn"
    };

    for (
        size_t i = 0;
        i < sizeof(marks) / sizeof(marks[0]);
        ++i
    ) {
        if (
            cleanup_connection_mark(
                ros,
                marks[i],
                &stats->marked_connections
            ) < 0
        ) {
            return -1;
        }
    }

    return 0;
}

int susanin_cleanup_legacy_state(
    ros_client_t *ros,
    susanin_cleanup_stats_t *stats
) {
    if (!ros || !stats) return -1;

    memset(
        stats,
        0,
        sizeof(*stats)
    );

    return cleanup_legacy_into(
        ros,
        stats
    );
}

int susanin_cleanup_dev2_runtime(
    ros_client_t *ros,
    susanin_cleanup_stats_t *stats
) {
    if (!ros || !stats) return -1;

    memset(
        stats,
        0,
        sizeof(*stats)
    );

    /*
     * Disable the lookup path by removing lazy mark rules first.
     * State can then be removed without any new per-port connections
     * being marked from those buckets.
     */
    if (
        cleanup_lazy_rules(
            ros,
            &stats->lazy_rules
        ) < 0
    ) {
        return -1;
    }

    if (
        cleanup_port_lists(
            ros,
            &stats->port_entries
        ) < 0
    ) {
        return -1;
    }

    return cleanup_legacy_into(
        ros,
        stats
    );
}

static int attr_true(
    const char *value
) {
    return
        value &&
        *value &&
        (
            strcmp(value, "true") == 0 ||
            strcmp(value, "yes") == 0
        );
}

static int disable_auto_rules(
    ros_client_t *ros,
    const id_vec_t *ids
) {
    for (
        size_t i = 0;
        i < ids->count;
        ++i
    ) {
        char wid[96];

        snprintf(
            wid,
            sizeof(wid),
            "=.id=%s",
            ids->items[i]
        );

        const char *cmd[] = {
            "/ip/firewall/mangle/set",
            wid,
            "=disabled=yes"
        };

        /*
         * Verify the complete namespace after all attempts instead of
         * treating a transient object race as an immediate failure.
         */
        (void)ros_command(
            ros,
            cmd,
            3,
            NULL,
            NULL
        );
    }

    return 0;
}

typedef struct {
    unsigned enabled;
} enabled_ctx_t;

static int enabled_auto_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    enabled_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *comment =
        ros_get_attr(s, "comment");

    if (
        !match_prefix(
            comment,
            "AUTO-AWG:"
        )
    ) {
        return 0;
    }

    if (
        !attr_true(
            ros_get_attr(
                s,
                "disabled"
            )
        )
    ) {
        ctx->enabled++;
    }

    return 0;
}

int susanin_disable_adaptive_mangle(
    ros_client_t *ros
) {
    if (!ros) return -1;

    id_vec_t ids;

    if (
        collect_ids(
            ros,
            "/ip/firewall/mangle/print",
            "=.proplist=.id,comment",
            NULL,
            "comment",
            match_prefix,
            "AUTO-AWG:",
            &ids
        ) < 0
    ) {
        return -1;
    }

    (void)disable_auto_rules(
        ros,
        &ids
    );

    id_vec_free(&ids);

    enabled_ctx_t verify = {0};

    const char *cmd[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=comment,disabled"
    };

    if (
        ros_command(
            ros,
            cmd,
            2,
            enabled_auto_cb,
            &verify
        ) < 0
    ) {
        return -1;
    }

    return
        verify.enabled == 0
            ? 0
            : -1;
}
