#include "target.h"

#include <stdio.h>
#include <string.h>

#define MAX_TARGET_IFS 128

typedef struct {
    char name[128];
    char type[64];
    int running;
    int disabled;
} target_iface_t;

typedef struct {
    target_iface_t ifs[MAX_TARGET_IFS];
    size_t count;
} iface_db_t;

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

static int iface_db_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    iface_db_t *db = opaque;

    if (
        !db ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    if (
        db->count >= MAX_TARGET_IFS
    ) {
        return 0;
    }

    const char *name =
        ros_get_attr(
            s,
            "name"
        );

    if (
        !name ||
        !*name
    ) {
        return 0;
    }

    target_iface_t *i =
        &db->ifs[db->count++];

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
        ros_get_attr(
            s,
            "type"
        )
            ? ros_get_attr(
                s,
                "type"
            )
            : "?"
    );

    i->running =
        bool_true(
            ros_get_attr(
                s,
                "running"
            )
        );

    i->disabled =
        bool_true(
            ros_get_attr(
                s,
                "disabled"
            )
        );

    return 0;
}

static int load_ifaces(
    ros_client_t *ros,
    iface_db_t *db
) {
    memset(
        db,
        0,
        sizeof(*db)
    );

    const char *cmd[] = {
        "/interface/print",
        "=.proplist=name,type,running,disabled"
    };

    return ros_command(
        ros,
        cmd,
        2,
        iface_db_cb,
        db
    );
}

static const target_iface_t *find_iface(
    const iface_db_t *db,
    const char *name
) {
    if (
        !db ||
        !name
    ) {
        return NULL;
    }

    for (
        size_t i = 0;
        i < db->count;
        ++i
    ) {
        if (
            strcmp(
                db->ifs[i].name,
                name
            ) == 0
        ) {
            return &db->ifs[i];
        }
    }

    return NULL;
}

static int gateway_uses_iface(
    const char *gateway,
    const char *iface
) {
    if (
        !gateway ||
        !iface
    ) {
        return 0;
    }

    if (
        strcmp(
            gateway,
            iface
        ) == 0
    ) {
        return 1;
    }

    const char *pct =
        strrchr(
            gateway,
            '%'
        );

    return
        pct &&
        strcmp(
            pct + 1,
            iface
        ) == 0;
}

typedef struct {
    const char *iface;
    char table[128];
    int found;
    int ambiguous;
} iface_route_ctx_t;

static int iface_route_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    iface_route_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *dst =
        ros_get_attr(
            s,
            "dst-address"
        );

    const char *table =
        ros_get_attr(
            s,
            "routing-table"
        );

    const char *gateway =
        ros_get_attr(
            s,
            "gateway"
        );

    const char *immediate =
        ros_get_attr(
            s,
            "immediate-gw"
        );

    const char *disabled =
        ros_get_attr(
            s,
            "disabled"
        );

    if (
        !dst ||
        strcmp(
            dst,
            "0.0.0.0/0"
        ) != 0 ||
        !table ||
        strcmp(
            table,
            "main"
        ) == 0 ||
        bool_true(disabled)
    ) {
        return 0;
    }

    if (
        !gateway_uses_iface(
            gateway,
            ctx->iface
        ) &&
        !gateway_uses_iface(
            immediate,
            ctx->iface
        )
    ) {
        return 0;
    }

    if (!ctx->found) {
        snprintf(
            ctx->table,
            sizeof(ctx->table),
            "%s",
            table
        );

        ctx->found = 1;
    } else if (
        strcmp(
            ctx->table,
            table
        ) != 0
    ) {
        ctx->ambiguous = 1;
    }

    return 0;
}

typedef struct {
    int found;
} table_found_ctx_t;

static int susanin_table_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    table_found_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *name =
        ros_get_attr(
            s,
            "name"
        );

    if (
        name &&
        strcmp(
            name,
            "susanin"
        ) == 0
    ) {
        ctx->found = 1;
    }

    return 0;
}

typedef struct {
    const char *iface;
    int found;
    int conflict;
} dedicated_route_ctx_t;

static int dedicated_route_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    dedicated_route_ctx_t *ctx =
        opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *dst =
        ros_get_attr(
            s,
            "dst-address"
        );

    const char *table =
        ros_get_attr(
            s,
            "routing-table"
        );

    const char *gateway =
        ros_get_attr(
            s,
            "gateway"
        );

    const char *disabled =
        ros_get_attr(
            s,
            "disabled"
        );

    if (
        !dst ||
        !table ||
        strcmp(
            dst,
            "0.0.0.0/0"
        ) != 0 ||
        strcmp(
            table,
            "susanin"
        ) != 0 ||
        bool_true(disabled)
    ) {
        return 0;
    }

    if (
        gateway_uses_iface(
            gateway,
            ctx->iface
        )
    ) {
        ctx->found = 1;
    } else {
        ctx->conflict = 1;
    }

    return 0;
}

static int ensure_dedicated_table(
    ros_client_t *ros,
    const char *iface,
    char out[128]
) {
    table_found_ctx_t table_ctx;

    memset(
        &table_ctx,
        0,
        sizeof(table_ctx)
    );

    const char *tables[] = {
        "/routing/table/print",
        "=.proplist=name,disabled"
    };

    if (
        ros_command(
            ros,
            tables,
            2,
            susanin_table_cb,
            &table_ctx
        ) < 0
    ) {
        return -1;
    }

    if (!table_ctx.found) {
        const char *add[] = {
            "/routing/table/add",
            "=name=susanin",
            "=fib=yes"
        };

        if (
            ros_command(
                ros,
                add,
                3,
                NULL,
                NULL
            ) < 0
        ) {
            return -1;
        }
    }

    dedicated_route_ctx_t route_ctx;

    memset(
        &route_ctx,
        0,
        sizeof(route_ctx)
    );

    route_ctx.iface = iface;

    const char *routes[] = {
        "/ip/route/print",
        "=.proplist=dst-address,gateway,routing-table,disabled"
    };

    if (
        ros_command(
            ros,
            routes,
            2,
            dedicated_route_cb,
            &route_ctx
        ) < 0
    ) {
        return -1;
    }

    if (
        route_ctx.conflict &&
        !route_ctx.found
    ) {
        fprintf(
            stderr,
            "Target: table 'susanin' already contains "
            "a conflicting default route.\n"
        );

        return -1;
    }

    if (!route_ctx.found) {
        char wgw[192];

        snprintf(
            wgw,
            sizeof(wgw),
            "=gateway=%s",
            iface
        );

        const char *add[] = {
            "/ip/route/add",
            "=dst-address=0.0.0.0/0",
            wgw,
            "=routing-table=susanin",
            "=distance=1",
            "=comment=SUSANIN: default route via selected target"
        };

        if (
            ros_command(
                ros,
                add,
                6,
                NULL,
                NULL
            ) < 0
        ) {
            return -1;
        }
    }

    snprintf(
        out,
        128,
        "susanin"
    );

    return 0;
}

typedef struct {
    char names[128][128];
    size_t count;
} table_list_ctx_t;

static int table_list_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    table_list_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *name =
        ros_get_attr(
            s,
            "name"
        );

    const char *disabled =
        ros_get_attr(
            s,
            "disabled"
        );

    if (
        !name ||
        !*name ||
        bool_true(disabled) ||
        ctx->count >= 128
    ) {
        return 0;
    }

    snprintf(
        ctx->names[ctx->count++],
        128,
        "%s",
        name
    );

    return 0;
}

typedef struct {
    const char *wanted_table;
    const iface_db_t *ifaces;

    int default_found;
    int egress_found;
    int ambiguous;

    char egress[128];
} table_route_ctx_t;

static const char *extract_iface(
    const iface_db_t *db,
    const char *gateway
) {
    if (
        !db ||
        !gateway ||
        !*gateway
    ) {
        return NULL;
    }

    const char *pct =
        strrchr(
            gateway,
            '%'
        );

    const char *candidate =
        pct
            ? pct + 1
            : gateway;

    const target_iface_t *i =
        find_iface(
            db,
            candidate
        );

    return i
        ? i->name
        : NULL;
}

static int table_route_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    table_route_ctx_t *ctx = opaque;

    if (
        !ctx ||
        !ros_is_reply(
            s,
            "!re"
        )
    ) {
        return 0;
    }

    const char *dst =
        ros_get_attr(
            s,
            "dst-address"
        );

    const char *table =
        ros_get_attr(
            s,
            "routing-table"
        );

    const char *disabled =
        ros_get_attr(
            s,
            "disabled"
        );

    if (
        !dst ||
        strcmp(
            dst,
            "0.0.0.0/0"
        ) != 0 ||
        !table ||
        strcmp(
            table,
            ctx->wanted_table
        ) != 0 ||
        bool_true(disabled)
    ) {
        return 0;
    }

    ctx->default_found = 1;

    const char *iface =
        extract_iface(
            ctx->ifaces,
            ros_get_attr(
                s,
                "immediate-gw"
            )
        );

    if (!iface) {
        iface =
            extract_iface(
                ctx->ifaces,
                ros_get_attr(
                    s,
                    "gateway"
                )
            );
    }

    if (!iface) {
        return 0;
    }

    if (!ctx->egress_found) {
        ctx->egress_found = 1;

        snprintf(
            ctx->egress,
            sizeof(ctx->egress),
            "%s",
            iface
        );
    } else if (
        strcmp(
            ctx->egress,
            iface
        ) != 0
    ) {
        ctx->ambiguous = 1;
    }

    return 0;
}

void target_show(
    const app_config_t *cfg
) {
    if (!cfg) return;

    printf(
        "=== SUSANIN ROUTING TARGET ===\n\n"
    );

    printf(
        "Mode            : %s\n",
        config_target_mode_name(
            cfg->target_mode
        )
    );

    printf(
        "Selected target : %s\n",
        cfg->target_value
            ? cfg->target_value
            : "<not selected>"
    );

    printf(
        "Resolved egress : %s\n",
        cfg->egress_interface
            ? cfg->egress_interface
            : "<not resolved>"
    );

    printf(
        "Resolved table  : %s\n",
        cfg->routing_table
            ? cfg->routing_table
            : "<not resolved>"
    );
}

int target_list(
    ros_client_t *ros
) {
    iface_db_t ifaces;

    if (
        load_ifaces(
            ros,
            &ifaces
        ) < 0
    ) {
        return -1;
    }

    printf(
        "=== ROUTING TARGET CANDIDATES ===\n\n"
        "Interfaces:\n"
    );

    for (
        size_t i = 0;
        i < ifaces.count;
        ++i
    ) {
        if (
            ifaces.ifs[i].disabled
        ) {
            continue;
        }

        printf(
            "  %-24s type=%-12s %s\n",
            ifaces.ifs[i].name,
            ifaces.ifs[i].type,
            ifaces.ifs[i].running
                ? "running"
                : "not-running"
        );
    }

    table_list_ctx_t tables;

    memset(
        &tables,
        0,
        sizeof(tables)
    );

    const char *cmd[] = {
        "/routing/table/print",
        "=.proplist=name,disabled"
    };

    if (
        ros_command(
            ros,
            cmd,
            2,
            table_list_cb,
            &tables
        ) < 0
    ) {
        return -1;
    }

    printf(
        "\nRouting tables:\n"
    );

    for (
        size_t i = 0;
        i < tables.count;
        ++i
    ) {
        printf(
            "  %s\n",
            tables.names[i]
        );
    }

    return 0;
}

int target_set_interface(
    ros_client_t *ros,
    app_config_t *cfg,
    const char *name
) {
    iface_db_t ifaces;

    if (
        load_ifaces(
            ros,
            &ifaces
        ) < 0
    ) {
        return -1;
    }

    const target_iface_t *iface =
        find_iface(
            &ifaces,
            name
        );

    if (!iface) {
        fprintf(
            stderr,
            "Target interface not found: %s\n",
            name
        );

        return -1;
    }

    if (iface->disabled) {
        fprintf(
            stderr,
            "Target interface is disabled: %s\n",
            name
        );

        return -1;
    }

    iface_route_ctx_t route_ctx;

    memset(
        &route_ctx,
        0,
        sizeof(route_ctx)
    );

    route_ctx.iface = name;

    const char *routes[] = {
        "/ip/route/print",
        "=.proplist=dst-address,gateway,immediate-gw,routing-table,disabled"
    };

    if (
        ros_command(
            ros,
            routes,
            2,
            iface_route_cb,
            &route_ctx
        ) < 0
    ) {
        return -1;
    }

    char table[128];

    memset(
        table,
        0,
        sizeof(table)
    );

    if (
        route_ctx.found &&
        !route_ctx.ambiguous
    ) {
        snprintf(
            table,
            sizeof(table),
            "%s",
            route_ctx.table
        );
    } else {
        if (
            ensure_dedicated_table(
                ros,
                name,
                table
            ) < 0
        ) {
            return -1;
        }
    }

    if (
        config_save_target(
            cfg,
            SUSANIN_TARGET_INTERFACE,
            name,
            name,
            table
        ) < 0
    ) {
        return -1;
    }

    printf(
        "Routing target saved:\n"
        "  mode      : interface\n"
        "  interface : %s\n"
        "  table     : %s\n",
        name,
        table
    );

    if (!iface->running) {
        printf(
            "WARNING: selected interface is currently not running.\n"
        );
    }

    return 0;
}

int target_set_routing_table(
    ros_client_t *ros,
    app_config_t *cfg,
    const char *name
) {
    if (
        !name ||
        !*name ||
        strcmp(
            name,
            "main"
        ) == 0
    ) {
        fprintf(
            stderr,
            "Target routing table must be a non-main table.\n"
        );

        return -1;
    }

    table_list_ctx_t tables;

    memset(
        &tables,
        0,
        sizeof(tables)
    );

    const char *table_cmd[] = {
        "/routing/table/print",
        "=.proplist=name,disabled"
    };

    if (
        ros_command(
            ros,
            table_cmd,
            2,
            table_list_cb,
            &tables
        ) < 0
    ) {
        return -1;
    }

    int table_found = 0;

    for (
        size_t i = 0;
        i < tables.count;
        ++i
    ) {
        if (
            strcmp(
                tables.names[i],
                name
            ) == 0
        ) {
            table_found = 1;
            break;
        }
    }

    if (!table_found) {
        fprintf(
            stderr,
            "Routing table not found: %s\n",
            name
        );

        return -1;
    }

    iface_db_t ifaces;

    if (
        load_ifaces(
            ros,
            &ifaces
        ) < 0
    ) {
        return -1;
    }

    table_route_ctx_t route_ctx;

    memset(
        &route_ctx,
        0,
        sizeof(route_ctx)
    );

    route_ctx.wanted_table = name;
    route_ctx.ifaces = &ifaces;

    const char *routes[] = {
        "/ip/route/print",
        "=.proplist=dst-address,gateway,immediate-gw,routing-table,disabled"
    };

    if (
        ros_command(
            ros,
            routes,
            2,
            table_route_cb,
            &route_ctx
        ) < 0
    ) {
        return -1;
    }

    if (!route_ctx.default_found) {
        fprintf(
            stderr,
            "Routing table '%s' has no enabled IPv4 default route.\n",
            name
        );

        return -1;
    }

    /*
     * dev1 deliberately resolves one interface from the selected table
     * so the unchanged v0.11.5 health/NAT data plane remains compatible.
     *
     * Later 0.12 builds will remove this restriction and health-check
     * the table itself.
     */
    if (
        !route_ctx.egress_found ||
        route_ctx.ambiguous
    ) {
        fprintf(
            stderr,
            "Routing table '%s' cannot currently be mapped to one "
            "unambiguous egress interface. dev1 refuses to change "
            "the proven v0.11.5 data-plane contract.\n",
            name
        );

        return -1;
    }

    if (
        config_save_target(
            cfg,
            SUSANIN_TARGET_ROUTING_TABLE,
            name,
            route_ctx.egress,
            name
        ) < 0
    ) {
        return -1;
    }

    printf(
        "Routing target saved:\n"
        "  mode      : routing-table\n"
        "  table     : %s\n"
        "  egress    : %s\n",
        name,
        route_ctx.egress
    );

    return 0;
}
