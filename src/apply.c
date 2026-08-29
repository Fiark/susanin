#include "apply.h"
#include "fingerprint.h"
#include "renderer.h"

#include <stdio.h>
#include <string.h>

#define MAX_CURRENT_MANGLE 32

typedef struct {
    const char *name;
    const char *interval;
} expected_scheduler_t;

typedef struct {
    const char *comment;
    const char *action;
    const char *protocol;
    const char *dst_list;
    const char *conn_mark;
    const char *new_conn_mark;
    const char *new_route_mark;
    const char *dst_port;
} expected_mangle_t;

typedef struct {
    char comment[96];
    char action[32];
    char protocol[16];
    char dst_list[64];
    char conn_mark[64];
    char new_conn_mark[64];
    char new_route_mark[64];
    char dst_port[32];
    char in_list[64];
    char disabled[16];
} current_mangle_t;

typedef struct {
    const app_config_t *cfg;
    unsigned lan_members;
    int egress_found;
    int egress_running;
    int table_found;

    int script_present[4];
    size_t script_bytes[4];
    char script_fp[4][17];
    int scheduler_present[4];
    int scheduler_ok[4];

    current_mangle_t mangle[MAX_CURRENT_MANGLE];
    size_t mangle_count;
} apply_ctx_t;

static const char *script_names[4] = {
    "auto-awg-health",
    "auto-awg-fast",
    "auto-awg-detect",
    "auto-awg-judge"
};

static const expected_scheduler_t schedulers[4] = {
    {"auto-awg-health", "3s"},
    {"auto-awg-fast",   "1s"},
    {"auto-awg-detect", "2s"},
    {"auto-awg-judge",  "1s"}
};

static const expected_mangle_t mangle_expected[] = {
    {"AUTO-AWG: L2 mark confirmed",     "mark-connection", "tcp", "auto_awg_ok_tcp",   "no-mark", "auto-awg-ok-conn",   NULL,       NULL},
    {"AUTO-AWG: L1 mark test",          "mark-connection", "tcp", "auto_awg_test_tcp", "no-mark", "auto-awg-test-conn", NULL,       NULL},
    {"AUTO-AWG: L2 mark confirmed UDP", "mark-connection", "udp", "auto_awg_ok_udp",   "no-mark", "auto-awg-ok-conn",   NULL,       NULL},
    {"AUTO-AWG: L1 mark test UDP",      "mark-connection", "udp", "auto_awg_test_udp", "no-mark", "auto-awg-test-conn", NULL,       NULL},
    {"AUTO-AWG: L2 route confirmed",    "mark-routing",    NULL,  NULL,                "auto-awg-ok-conn",   NULL, NULL, NULL},
    {"AUTO-AWG: L1 route test",         "mark-routing",    NULL,  NULL,                "auto-awg-test-conn", NULL, NULL, NULL},
    {"AUTO-AWG: router DNS UDP via tunnel", "mark-routing", "udp", NULL, NULL, NULL, NULL, "53"},
    {"AUTO-AWG: router DNS TCP via tunnel", "mark-routing", "tcp", NULL, NULL, NULL, NULL, "53"}
};

static int attr_false(const char *v) {
    return !v || !*v || strcmp(v, "false") == 0 || strcmp(v, "no") == 0;
}

static int str_eq_opt(const char *actual, const char *expected) {
    if (!expected) return 1;
    return actual && strcmp(actual, expected) == 0;
}

static int find_name(const char *name, const char *const *names, size_t n) {
    if (!name) return -1;
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(name, names[i]) == 0) return (int)i;
    }
    return -1;
}

static int lan_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *iface = ros_get_attr(s, "interface");
    if (iface && *iface) ctx->lan_members++;
    return 0;
}

static int iface_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (!name || !ctx->cfg->egress_interface) return 0;
    if (strcmp(name, ctx->cfg->egress_interface) != 0) return 0;
    ctx->egress_found = 1;
    const char *running = ros_get_attr(s, "running");
    const char *disabled = ros_get_attr(s, "disabled");
    ctx->egress_running = running && strcmp(running, "true") == 0 && attr_false(disabled);
    return 0;
}

static int table_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (name && ctx->cfg->routing_table && strcmp(name, ctx->cfg->routing_table) == 0) {
        ctx->table_found = 1;
    }
    return 0;
}

static int script_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    int idx = find_name(name, script_names, 4);
    if (idx >= 0) {
        const char *source = ros_get_attr(s, "source");
        ctx->script_present[idx] = 1;
        ctx->script_bytes[idx] = source ? strlen(source) : 0;
        susanin_fingerprint_hex(source, ctx->script_fp[idx]);
    }
    return 0;
}

static int scheduler_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (!name) return 0;
    for (size_t i = 0; i < 4; ++i) {
        if (strcmp(name, schedulers[i].name) != 0) continue;
        ctx->scheduler_present[i] = 1;
        const char *interval = ros_get_attr(s, "interval");
        const char *event = ros_get_attr(s, "on-event");
        const char *disabled = ros_get_attr(s, "disabled");
        ctx->scheduler_ok[i] = interval && strcmp(interval, schedulers[i].interval) == 0 &&
                               event && strcmp(event, schedulers[i].name) == 0 && attr_false(disabled);
        break;
    }
    return 0;
}

static void copy_attr(char *dst, size_t cap, const char *v) {
    if (!dst || cap == 0) return;
    snprintf(dst, cap, "%s", v ? v : "");
}

static int mangle_cb(const ros_sentence_t *s, void *opaque) {
    apply_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *comment = ros_get_attr(s, "comment");
    if (!comment || strncmp(comment, "AUTO-AWG:", 9) != 0) return 0;
    if (ctx->mangle_count >= MAX_CURRENT_MANGLE) return 0;

    current_mangle_t *m = &ctx->mangle[ctx->mangle_count++];
    memset(m, 0, sizeof(*m));
    copy_attr(m->comment, sizeof(m->comment), comment);
    copy_attr(m->action, sizeof(m->action), ros_get_attr(s, "action"));
    copy_attr(m->protocol, sizeof(m->protocol), ros_get_attr(s, "protocol"));
    copy_attr(m->dst_list, sizeof(m->dst_list), ros_get_attr(s, "dst-address-list"));
    copy_attr(m->conn_mark, sizeof(m->conn_mark), ros_get_attr(s, "connection-mark"));
    copy_attr(m->new_conn_mark, sizeof(m->new_conn_mark), ros_get_attr(s, "new-connection-mark"));
    copy_attr(m->new_route_mark, sizeof(m->new_route_mark), ros_get_attr(s, "new-routing-mark"));
    copy_attr(m->dst_port, sizeof(m->dst_port), ros_get_attr(s, "dst-port"));
    copy_attr(m->in_list, sizeof(m->in_list), ros_get_attr(s, "in-interface-list"));
    copy_attr(m->disabled, sizeof(m->disabled), ros_get_attr(s, "disabled"));
    return 0;
}

static const current_mangle_t *find_mangle(const apply_ctx_t *ctx, const char *comment) {
    for (size_t i = 0; i < ctx->mangle_count; ++i) {
        if (strcmp(ctx->mangle[i].comment, comment) == 0) return &ctx->mangle[i];
    }
    return NULL;
}

static int mangle_matches(const current_mangle_t *m, const expected_mangle_t *e,
                          const app_config_t *cfg) {
    if (!m || !e) return 0;
    if (strcmp(m->action, e->action) != 0) return 0;
    if (!str_eq_opt(m->protocol[0] ? m->protocol : NULL, e->protocol)) return 0;
    if (!str_eq_opt(m->dst_list[0] ? m->dst_list : NULL, e->dst_list)) return 0;
    if (!str_eq_opt(m->conn_mark[0] ? m->conn_mark : NULL, e->conn_mark)) return 0;
    if (!str_eq_opt(m->new_conn_mark[0] ? m->new_conn_mark : NULL, e->new_conn_mark)) return 0;
    if (!str_eq_opt(m->dst_port[0] ? m->dst_port : NULL, e->dst_port)) return 0;
    if (!attr_false(m->disabled[0] ? m->disabled : NULL)) return 0;

    if (strncmp(e->comment, "AUTO-AWG: L2 route", 18) == 0 ||
        strncmp(e->comment, "AUTO-AWG: L1 route", 18) == 0 ||
        strstr(e->comment, "router DNS")) {
        if (!cfg->routing_table || strcmp(m->new_route_mark, cfg->routing_table) != 0) return 0;
    }

    if (strncmp(e->comment, "AUTO-AWG: L", 11) == 0 && !strstr(e->comment, "router DNS")) {
        if (!cfg->lan_list || strcmp(m->in_list, cfg->lan_list) != 0) return 0;
    }

    return 1;
}

static int collect(ros_client_t *ros, apply_ctx_t *ctx) {
    char qlist[256];
    snprintf(qlist, sizeof(qlist), "?list=%s", ctx->cfg->lan_list);
    const char *members[] = {"/interface/list/member/print", "=.proplist=interface", qlist};
    if (ros_command(ros, members, 3, lan_cb, ctx) < 0) return -1;

    const char *ifs[] = {"/interface/print", "=.proplist=name,running,disabled"};
    if (ros_command(ros, ifs, 2, iface_cb, ctx) < 0) return -1;

    const char *tables[] = {"/routing/table/print", "=.proplist=name"};
    if (ros_command(ros, tables, 2, table_cb, ctx) < 0) return -1;

    const char *scripts[] = {"/system/script/print", "=.proplist=name,source"};
    if (ros_command(ros, scripts, 2, script_cb, ctx) < 0) return -1;

    const char *sched[] = {"/system/scheduler/print", "=.proplist=name,interval,on-event,disabled"};
    if (ros_command(ros, sched, 2, scheduler_cb, ctx) < 0) return -1;

    const char *mangle[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=comment,action,protocol,dst-address-list,connection-mark,new-connection-mark,new-routing-mark,dst-port,in-interface-list,disabled"
    };
    if (ros_command(ros, mangle, 2, mangle_cb, ctx) < 0) return -1;

    return 0;
}

int apply_dry_run(ros_client_t *ros, const app_config_t *cfg) {
    apply_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;

    if (!cfg->egress_interface || !*cfg->egress_interface ||
        !cfg->routing_table || !*cfg->routing_table) {
        fprintf(stderr, "Susanin apply --dry-run requires SUSANIN_EGRESS_INTERFACE and SUSANIN_ROUTING_TABLE\n");
        return -1;
    }

    if (collect(ros, &ctx) < 0) return -1;

    unsigned create = 0;
    unsigned update = 0;
    unsigned keep = 0;
    unsigned blockers = 0;

    printf("=== SUSANIN APPLY DRY-RUN ===\n");
    printf("Mode: read-only structural reconciliation\n");
    printf("RouterOS changes: NONE\n\n");

    printf("Prerequisites:\n");
    printf("  [%s] LAN interface-list '%s' members=%u\n",
           ctx.lan_members ? "OK" : "BLOCK", cfg->lan_list, ctx.lan_members);
    if (!ctx.lan_members) blockers++;

    printf("  [%s] egress '%s'%s\n",
           ctx.egress_found && ctx.egress_running ? "OK" : "BLOCK",
           cfg->egress_interface,
           !ctx.egress_found ? " not found" : (ctx.egress_running ? " running" : " not running/disabled"));
    if (!(ctx.egress_found && ctx.egress_running)) blockers++;

    printf("  [%s] routing table '%s'%s\n",
           ctx.table_found ? "OK" : "BLOCK", cfg->routing_table,
           ctx.table_found ? "" : " not found");
    if (!ctx.table_found) blockers++;

    susanin_render_bundle_t desired;
    int desired_ok = renderer_build(ros, cfg, &desired) == 0;
    if (!desired_ok) blockers++;

    printf("\nScripts (generated desired source; legacy RouterOS names retained for compatibility):\n");
    for (size_t i = 0; i < 4; ++i) {
        if (!desired_ok) {
            printf("  BLOCK  %-18s desired source could not be rendered\n", script_names[i]);
            continue;
        }
        if (!ctx.script_present[i]) {
            printf("  CREATE %-18s desired bytes=%-6zu fnv1a64=%s\n",
                   script_names[i], desired.scripts[i].bytes, desired.scripts[i].fp);
            create++;
        } else if (ctx.script_bytes[i] != desired.scripts[i].bytes || strcmp(ctx.script_fp[i], desired.scripts[i].fp) != 0) {
            printf("  UPDATE %-18s current=%zu/%s desired=%zu/%s\n",
                   script_names[i], ctx.script_bytes[i], ctx.script_fp[i],
                   desired.scripts[i].bytes, desired.scripts[i].fp);
            update++;
        } else {
            printf("  KEEP   %-18s bytes=%-6zu fnv1a64=%s\n",
                   script_names[i], ctx.script_bytes[i], ctx.script_fp[i]);
            keep++;
        }
    }
    printf("  note: desired source is rendered dynamically from discovered LAN IPv4 networks and selected egress. Write-mode remains disabled.\n");

    printf("\nSchedulers:\n");
    for (size_t i = 0; i < 4; ++i) {
        if (!ctx.scheduler_present[i]) {
            printf("  CREATE %-18s interval=%s\n", schedulers[i].name, schedulers[i].interval);
            create++;
        } else if (!ctx.scheduler_ok[i]) {
            printf("  UPDATE %-18s -> interval=%s on-event=%s enabled\n",
                   schedulers[i].name, schedulers[i].interval, schedulers[i].name);
            update++;
        } else {
            printf("  KEEP   %-18s interval=%s\n", schedulers[i].name, schedulers[i].interval);
            keep++;
        }
    }

    printf("\nMangle:\n");
    for (size_t i = 0; i < sizeof(mangle_expected) / sizeof(mangle_expected[0]); ++i) {
        expected_mangle_t e = mangle_expected[i];
        if (e.action && strcmp(e.action, "mark-routing") == 0) {
            e.new_route_mark = cfg->routing_table;
        }
        const current_mangle_t *m = find_mangle(&ctx, e.comment);
        if (!m) {
            printf("  CREATE %s\n", e.comment);
            create++;
        } else if (!mangle_matches(m, &e, cfg)) {
            printf("  UPDATE %s\n", e.comment);
            update++;
        } else {
            printf("  KEEP   %s\n", e.comment);
            keep++;
        }
    }

    if (desired_ok) renderer_free(&desired);

    printf("\nSummary:\n");
    printf("  KEEP=%u CREATE=%u UPDATE=%u BLOCKERS=%u\n", keep, create, update, blockers);
    if (blockers) {
        printf("Result: BLOCKED - prerequisites must be fixed before apply.\n");
        return 0;
    }
    if (create == 0 && update == 0) {
        printf("Result: IN SYNC structurally.\n");
    } else {
        printf("Result: CHANGES WOULD BE REQUIRED. No changes were made.\n");
    }
    return 0;
}
