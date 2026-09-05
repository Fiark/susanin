#include "setup.h"

#include "install.h"
#include "target.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SETUP_IFACES 128
#define MAX_SETUP_TABLES 128
#define MAX_LAN_MEMBERS 64

typedef struct {
    char name[128];
} lan_member_t;

typedef struct {
    char name[128];
    char type[64];
    int running;
} setup_iface_t;

typedef struct {
    char name[128];
} setup_table_t;

typedef struct {
    lan_member_t lan[MAX_LAN_MEMBERS];
    size_t lan_count;

    setup_iface_t ifaces[MAX_SETUP_IFACES];
    size_t iface_count;

    setup_table_t tables[MAX_SETUP_TABLES];
    size_t table_count;

    const char *lan_list;
} setup_ctx_t;

typedef struct {
    char id[64];
    int found;
} named_object_t;

typedef struct {
    int found;
    int disabled;
} ipv6_settings_t;

static int bool_true(
    const char *s
) {
    return
        s &&
        (
            strcmp(s, "true") == 0 ||
            strcmp(s, "yes") == 0
        );
}

static int named_object_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    named_object_t *o = opaque;

    if (
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *id =
        ros_get_attr(s, ".id");

    if (
        !id ||
        !*id
    ) {
        return 0;
    }

    o->found = 1;

    snprintf(
        o->id,
        sizeof(o->id),
        "%s",
        id
    );

    return 0;
}

static void best_effort_remove_named(
    ros_client_t *ros,
    const char *print_cmd,
    const char *remove_cmd,
    const char *name
) {
    char q[192];

    snprintf(
        q,
        sizeof(q),
        "?name=%s",
        name
    );

    named_object_t o;
    memset(&o, 0, sizeof(o));

    const char *lookup[] = {
        print_cmd,
        "=.proplist=.id,name",
        q
    };

    if (
        ros_command(
            ros,
            lookup,
            3,
            named_object_cb,
            &o
        ) < 0 ||
        !o.found
    ) {
        return;
    }

    char wid[96];

    snprintf(
        wid,
        sizeof(wid),
        "=.id=%s",
        o.id
    );

    const char *remove[] = {
        remove_cmd,
        wid
    };

    (void)ros_command(
        ros,
        remove,
        2,
        NULL,
        NULL
    );
}

static int has_lan_member(
    const setup_ctx_t *ctx,
    const char *name
) {
    for (
        size_t i = 0;
        i < ctx->lan_count;
        ++i
    ) {
        if (
            strcmp(
                ctx->lan[i].name,
                name
            ) == 0
        ) {
            return 1;
        }
    }

    return 0;
}

static int lan_member_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    setup_ctx_t *ctx = opaque;

    if (
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *list =
        ros_get_attr(s, "list");

    const char *iface =
        ros_get_attr(s, "interface");

    const char *disabled =
        ros_get_attr(s, "disabled");

    if (
        !list ||
        !ctx->lan_list ||
        strcmp(
            list,
            ctx->lan_list
        ) != 0 ||
        !iface ||
        !*iface ||
        bool_true(disabled) ||
        ctx->lan_count >= MAX_LAN_MEMBERS ||
        has_lan_member(ctx, iface)
    ) {
        return 0;
    }

    snprintf(
        ctx->lan[ctx->lan_count++].name,
        sizeof(ctx->lan[0].name),
        "%s",
        iface
    );

    return 0;
}

static int setup_iface_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    setup_ctx_t *ctx = opaque;

    if (
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *name =
        ros_get_attr(s, "name");

    if (
        !name ||
        !*name ||
        ctx->iface_count >= MAX_SETUP_IFACES
    ) {
        return 0;
    }

    if (
        bool_true(
            ros_get_attr(
                s,
                "disabled"
            )
        )
    ) {
        return 0;
    }

    /*
     * Never route through the source LAN itself or through
     * Susanin controller plumbing.
     */
    if (
        has_lan_member(ctx, name) ||
        strcmp(name, "veth-susanin") == 0 ||
        strcmp(name, "bridge-susanin") == 0
    ) {
        return 0;
    }

    setup_iface_t *i =
        &ctx->ifaces[ctx->iface_count++];

    snprintf(
        i->name,
        sizeof(i->name),
        "%s",
        name
    );

    snprintf(
        i->type,
        sizeof(i->type),
        "%s",
        ros_get_attr(s, "type")
            ? ros_get_attr(s, "type")
            : "?"
    );

    i->running =
        bool_true(
            ros_get_attr(
                s,
                "running"
            )
        );

    return 0;
}

static int setup_table_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    setup_ctx_t *ctx = opaque;

    if (
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    const char *name =
        ros_get_attr(s, "name");

    if (
        !name ||
        !*name ||
        strcmp(name, "main") == 0 ||
        bool_true(
            ros_get_attr(
                s,
                "disabled"
            )
        ) ||
        ctx->table_count >= MAX_SETUP_TABLES
    ) {
        return 0;
    }

    snprintf(
        ctx->tables[
            ctx->table_count++
        ].name,
        sizeof(ctx->tables[0].name),
        "%s",
        name
    );

    return 0;
}

static int ipv6_settings_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    ipv6_settings_t *ctx = opaque;

    if (
        !ros_is_reply(s, "!re")
    ) {
        return 0;
    }

    ctx->found = 1;

    ctx->disabled =
        bool_true(
            ros_get_attr(
                s,
                "disable-ipv6"
            )
        );

    return 0;
}

static int enforce_ipv4_only(
    ros_client_t *ros
) {
    ipv6_settings_t before;
    memset(&before, 0, sizeof(before));

    const char *read_settings[] = {
        "/ipv6/settings/print",
        "=.proplist=disable-ipv6"
    };

    if (
        ros_command(
            ros,
            read_settings,
            2,
            ipv6_settings_cb,
            &before
        ) < 0 ||
        !before.found
    ) {
        fprintf(
            stderr,
            "Susanin setup: cannot inspect RouterOS IPv6 settings.\n"
        );

        return -1;
    }

    if (!before.disabled) {
        const char *disable[] = {
            "/ipv6/settings/set",
            "=disable-ipv6=yes"
        };

        if (
            ros_command(
                ros,
                disable,
                2,
                NULL,
                NULL
            ) < 0
        ) {
            fprintf(
                stderr,
                "Susanin setup: failed to enable strict IPv4-only mode.\n"
            );

            return -1;
        }
    }

    ipv6_settings_t after;
    memset(&after, 0, sizeof(after));

    if (
        ros_command(
            ros,
            read_settings,
            2,
            ipv6_settings_cb,
            &after
        ) < 0 ||
        !after.found ||
        !after.disabled
    ) {
        fprintf(
            stderr,
            "Susanin setup: IPv6 disable verification failed.\n"
        );

        return -1;
    }

    printf(
        "IPv4-only mode: RouterOS disable-ipv6=yes\n"
    );

    if (!before.disabled) {
        printf(
            "NOTE: a later RouterOS reboot may be required "
            "to purge already-created IPv6/link-local state. "
            "Susanin never reboots automatically.\n"
        );
    }

    return 0;
}

static int prompt_index(
    const char *prompt,
    size_t max,
    size_t *out
) {
    char line[128];

    for (;;) {
        printf("%s", prompt);
        fflush(stdout);

        if (
            !fgets(
                line,
                sizeof(line),
                stdin
            )
        ) {
            return -1;
        }

        char *end = NULL;

        long v =
            strtol(
                line,
                &end,
                10
            );

        while (
            end &&
            isspace(
                (unsigned char)*end
            )
        ) {
            ++end;
        }

        if (
            end &&
            *end == '\0' &&
            v >= 1 &&
            (size_t)v <= max
        ) {
            *out =
                (size_t)(v - 1);

            return 0;
        }

        printf(
            "Please enter a number from 1 to %zu.\n",
            max
        );
    }
}

static int load_setup_inventory(
    ros_client_t *ros,
    setup_ctx_t *ctx
) {
    const char *members[] = {
        "/interface/list/member/print",
        "=.proplist=list,interface,disabled"
    };

    if (
        ros_command(
            ros,
            members,
            2,
            lan_member_cb,
            ctx
        ) < 0
    ) {
        return -1;
    }

    if (ctx->lan_count == 0) {
        fprintf(
            stderr,
            "Susanin setup: interface-list '%s' has no usable members.\n",
            ctx->lan_list
        );

        return -1;
    }

    const char *ifaces[] = {
        "/interface/print",
        "=.proplist=name,type,running,disabled"
    };

    if (
        ros_command(
            ros,
            ifaces,
            2,
            setup_iface_cb,
            ctx
        ) < 0
    ) {
        return -1;
    }

    const char *tables[] = {
        "/routing/table/print",
        "=.proplist=name,fib,disabled"
    };

    if (
        ros_command(
            ros,
            tables,
            2,
            setup_table_cb,
            ctx
        ) < 0
    ) {
        return -1;
    }

    return 0;
}

int setup_run(
    ros_client_t *ros,
    app_config_t *cfg
) {
    if (
        !ros ||
        !cfg
    ) {
        return -1;
    }

    printf(
        "=== SUSANIN FIRST-RUN SETUP v0.12 ===\n"
    );

    printf(
        "LAN interface-list: %s\n",
        cfg->lan_list
            ? cfg->lan_list
            : "LAN"
    );

    printf(
        "Network model: IPv4-only\n\n"
    );

    if (
        enforce_ipv4_only(
            ros
        ) < 0
    ) {
        return -1;
    }

    setup_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.lan_list =
        cfg->lan_list
            ? cfg->lan_list
            : "LAN";

    if (
        load_setup_inventory(
            ros,
            &ctx
        ) < 0
    ) {
        return -1;
    }

    printf(
        "\nChoose routing target type:\n"
        "  1) Interface\n"
        "  2) Routing table\n"
    );

    size_t mode = 0;

    if (
        prompt_index(
            "Selection: ",
            2,
            &mode
        ) < 0
    ) {
        return -1;
    }

    int rc = -1;

    if (mode == 0) {
        if (ctx.iface_count == 0) {
            fprintf(
                stderr,
                "Susanin setup: no usable non-LAN interfaces found.\n"
            );

            return -1;
        }

        printf(
            "\nChoose interface:\n"
        );

        for (
            size_t i = 0;
            i < ctx.iface_count;
            ++i
        ) {
            printf(
                "  %zu) %-24s type=%-12s %s\n",
                i + 1,
                ctx.ifaces[i].name,
                ctx.ifaces[i].type,
                ctx.ifaces[i].running
                    ? "running"
                    : "NOT RUNNING"
            );
        }

        size_t selected = 0;

        if (
            prompt_index(
                "Selection: ",
                ctx.iface_count,
                &selected
            ) < 0
        ) {
            return -1;
        }

        rc =
            target_set_interface(
                ros,
                cfg,
                ctx.ifaces[selected].name
            );
    } else {
        if (ctx.table_count == 0) {
            fprintf(
                stderr,
                "Susanin setup: no non-main routing tables found.\n"
            );

            return -1;
        }

        printf(
            "\nChoose routing table:\n"
        );

        for (
            size_t i = 0;
            i < ctx.table_count;
            ++i
        ) {
            printf(
                "  %zu) %s\n",
                i + 1,
                ctx.tables[i].name
            );
        }

        size_t selected = 0;

        if (
            prompt_index(
                "Selection: ",
                ctx.table_count,
                &selected
            ) < 0
        ) {
            return -1;
        }

        rc =
            target_set_routing_table(
                ros,
                cfg,
                ctx.tables[selected].name
            );
    }

    if (rc < 0) {
        return -1;
    }

    printf(
        "\nInstalling/reconciling Susanin data-plane...\n"
    );

    if (
        install_run(
            ros,
            cfg,
            0
        ) < 0
    ) {
        fprintf(
            stderr,
            "Susanin setup: data-plane installation failed; "
            "bootstrap helper retained for retry.\n"
        );

        return -1;
    }

    best_effort_remove_named(
        ros,
        "/system/scheduler/print",
        "/system/scheduler/remove",
        "susanin-bootstrap-worker"
    );

    best_effort_remove_named(
        ros,
        "/system/script/print",
        "/system/script/remove",
        "susanin-bootstrap-worker"
    );

    best_effort_remove_named(
        ros,
        "/system/scheduler/print",
        "/system/scheduler/remove",
        "susanin-bootstrap-start"
    );

    best_effort_remove_named(
        ros,
        "/system/script/print",
        "/system/script/remove",
        "susanin-bootstrap-start"
    );

    printf(
        "\nSusanin setup saved.\n"
    );

    printf(
        "  Target mode : %s\n",
        config_target_mode_name(
            cfg->target_mode
        )
    );

    printf(
        "  Target      : %s\n",
        cfg->target_value
            ? cfg->target_value
            : "<none>"
    );

    printf(
        "  Egress      : %s\n",
        cfg->egress_interface
            ? cfg->egress_interface
            : "<table-native>"
    );

    printf(
        "  Table       : %s\n",
        cfg->routing_table
            ? cfg->routing_table
            : "<none>"
    );

    printf(
        "  IPv6        : disabled by Susanin strict IPv4-only mode\n"
    );

    printf(
        "Data-plane: installed or already in sync.\n"
    );

    return 0;
}
