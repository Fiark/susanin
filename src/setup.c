#include "setup.h"
#include "install.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CANDIDATES 64

typedef struct {
    char name[128];
    char type[64];
    int running;
} egress_candidate_t;

typedef struct {
    egress_candidate_t candidates[MAX_CANDIDATES];
    size_t candidate_count;
    const char *selected_if;
    char matched_table[128];
    unsigned matched_table_count;
    unsigned lan_members;
    const char *lan_list;
} setup_ctx_t;

typedef struct {
    char id[64];
    int found;
} named_object_t;

static int named_object_cb(const ros_sentence_t *s, void *opaque) {
    named_object_t *o = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *id = ros_get_attr(s, ".id");
    if (!id || !*id) return 0;
    o->found = 1;
    snprintf(o->id, sizeof(o->id), "%s", id);
    return 0;
}

static void best_effort_remove_named(ros_client_t *ros, const char *print_cmd,
                                     const char *remove_cmd, const char *name) {
    char q[192];
    snprintf(q, sizeof(q), "?name=%s", name);
    named_object_t o;
    memset(&o, 0, sizeof(o));
    const char *lookup[] = {print_cmd, "=.proplist=.id,name", q};
    if (ros_command(ros, lookup, 3, named_object_cb, &o) < 0 || !o.found) return;

    char wid[96];
    snprintf(wid, sizeof(wid), "=.id=%s", o.id);
    const char *remove[] = {remove_cmd, wid};
    (void)ros_command(ros, remove, 2, NULL, NULL);
}

static int bool_true(const char *s) {
    return s && (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0);
}

static int is_tunnel_type(const char *name, const char *type) {
    if (!name) return 0;
    if (type && *type && strcmp(type, "?") != 0) {
        static const char *types[] = {
            "wg", "ovpn-out", "sstp-out", "l2tp-out", "pptp-out",
            "ipip", "gre", "gre6", "eoip", "vxlan", "zerotier"
        };
        for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i) {
            if (strcmp(type, types[i]) == 0) return 1;
        }
        /* RouterOS supplied an explicit non-tunnel type (bridge, veth,
           ether, wifi, ...). Do not let a name such as "autoawg" turn it
           into a false VPN candidate merely because it contains "wg". */
        return 0;
    }
    return strncmp(name, "wg-", 3) == 0 ||
           strncmp(name, "vpn-", 4) == 0 ||
           strncmp(name, "ovpn-", 5) == 0 ||
           strncmp(name, "tun", 3) == 0 ||
           strstr(name, "wireguard") != NULL;
}

static int iface_cb(const ros_sentence_t *s, void *opaque) {
    setup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *type = ros_get_attr(s, "type");
    const char *running = ros_get_attr(s, "running");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!name || !is_tunnel_type(name, type)) return 0;
    if (bool_true(disabled)) return 0;
    if (ctx->candidate_count >= MAX_CANDIDATES) return 0;
    egress_candidate_t *c = &ctx->candidates[ctx->candidate_count++];
    snprintf(c->name, sizeof(c->name), "%s", name);
    snprintf(c->type, sizeof(c->type), "%s", type ? type : "?");
    c->running = bool_true(running);
    return 0;
}

static int lan_member_cb(const ros_sentence_t *s, void *opaque) {
    setup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *list = ros_get_attr(s, "list");
    const char *iface = ros_get_attr(s, "interface");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!list || !ctx->lan_list || strcmp(list, ctx->lan_list) != 0) return 0;
    if (!iface || !*iface || bool_true(disabled)) return 0;
    ctx->lan_members++;
    return 0;
}

static int route_cb(const ros_sentence_t *s, void *opaque) {
    setup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re") || !ctx->selected_if) return 0;
    const char *dst = ros_get_attr(s, "dst-address");
    const char *gw = ros_get_attr(s, "gateway");
    const char *table = ros_get_attr(s, "routing-table");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!dst || strcmp(dst, "0.0.0.0/0") != 0 || !gw || !table || strcmp(table, "main") == 0 || bool_true(disabled)) return 0;
    if (!strstr(gw, ctx->selected_if)) return 0;
    if (ctx->matched_table_count == 0) snprintf(ctx->matched_table, sizeof(ctx->matched_table), "%s", table);
    if (strcmp(ctx->matched_table, table) == 0) ctx->matched_table_count++;
    else ctx->matched_table_count += 1000; /* mark ambiguity */
    return 0;
}


typedef struct {
    int table_found;
    int route_found;
    int route_conflict;
    const char *selected_if;
} provision_ctx_t;

static int provision_table_cb(const ros_sentence_t *s, void *opaque) {
    provision_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (name && strcmp(name, "susanin") == 0) ctx->table_found = 1;
    return 0;
}

static int provision_route_cb(const ros_sentence_t *s, void *opaque) {
    provision_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *dst = ros_get_attr(s, "dst-address");
    const char *table = ros_get_attr(s, "routing-table");
    const char *gw = ros_get_attr(s, "gateway");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!dst || !table || strcmp(dst, "0.0.0.0/0") != 0 || strcmp(table, "susanin") != 0 || bool_true(disabled)) return 0;
    if (gw && ctx->selected_if && strstr(gw, ctx->selected_if)) ctx->route_found = 1;
    else ctx->route_conflict = 1;
    return 0;
}

static int ensure_dedicated_table(ros_client_t *ros, const char *selected_if, char out[128]) {
    provision_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.selected_if = selected_if;

    const char *tables[] = {"/routing/table/print", "=.proplist=name,disabled"};
    if (ros_command(ros, tables, 2, provision_table_cb, &ctx) < 0) return -1;
    if (!ctx.table_found) {
        const char *add_table[] = {"/routing/table/add", "=name=susanin", "=fib=yes"};
        if (ros_command(ros, add_table, 3, NULL, NULL) < 0) {
            fprintf(stderr, "Susanin setup: could not create dedicated routing table 'susanin'.\n");
            return -1;
        }
        printf("Routing table auto-created: susanin\n");
    }

    const char *routes[] = {"/ip/route/print", "=.proplist=dst-address,gateway,routing-table,disabled,comment"};
    if (ros_command(ros, routes, 2, provision_route_cb, &ctx) < 0) return -1;
    if (ctx.route_conflict && !ctx.route_found) {
        fprintf(stderr, "Susanin setup: routing table 'susanin' already has a conflicting active default route; refusing to repurpose it.\n");
        return -1;
    }
    if (!ctx.route_found) {
        char wgw[192];
        snprintf(wgw, sizeof(wgw), "=gateway=%s", selected_if);
        const char *add_route[] = {
            "/ip/route/add", "=dst-address=0.0.0.0/0", wgw,
            "=routing-table=susanin", "=distance=1",
            "=comment=SUSANIN: default route via selected tunnel"
        };
        if (ros_command(ros, add_route, 6, NULL, NULL) < 0) {
            fprintf(stderr, "Susanin setup: could not create default route in dedicated table.\n");
            return -1;
        }
        printf("Default tunnel route auto-created in table: susanin\n");
    }
    snprintf(out, 128, "susanin");
    return 0;
}

static int prompt_index(const char *prompt, size_t max, size_t *out) {
    char line[128];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) return -1;
        char *end = NULL;
        long v = strtol(line, &end, 10);
        while (end && isspace((unsigned char)*end)) ++end;
        if (end && *end == '\0' && v >= 1 && (size_t)v <= max) {
            *out = (size_t)(v - 1);
            return 0;
        }
        printf("Please enter a number from 1 to %zu.\n", max);
    }
}

int setup_run(ros_client_t *ros, app_config_t *cfg) {
    setup_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.lan_list = cfg->lan_list ? cfg->lan_list : "LAN";

    const char *members[] = {"/interface/list/member/print", "=.proplist=list,interface,disabled"};
    if (ros_command(ros, members, 2, lan_member_cb, &ctx) < 0) return -1;
    if (ctx.lan_members == 0) {
        fprintf(stderr, "Susanin setup: interface-list '%s' has no members. Advanced LAN selection is required.\n", cfg->lan_list);
        return -1;
    }

    const char *ifs[] = {"/interface/print", "=.proplist=name,type,running,disabled"};
    if (ros_command(ros, ifs, 2, iface_cb, &ctx) < 0) return -1;
    if (ctx.candidate_count == 0) {
        fprintf(stderr, "Susanin setup: no route-based tunnel/VPN interfaces detected.\n");
        return -1;
    }

    printf("=== SUSANIN FIRST-RUN SETUP ===\n");
    printf("LAN: interface-list=%s (%u member%s)\n\n", cfg->lan_list, ctx.lan_members, ctx.lan_members == 1 ? "" : "s");
    printf("Choose the VPN/tunnel interface where blocked traffic should go:\n");
    for (size_t i = 0; i < ctx.candidate_count; ++i) {
        printf("  %zu) %-24s type=%-10s %s\n", i + 1, ctx.candidates[i].name,
               ctx.candidates[i].type, ctx.candidates[i].running ? "running" : "NOT RUNNING");
    }
    size_t selected = 0;
    if (prompt_index("Selection: ", ctx.candidate_count, &selected) < 0) return -1;
    ctx.selected_if = ctx.candidates[selected].name;

    const char *routes[] = {"/ip/route/print", "=.proplist=dst-address,gateway,routing-table,disabled"};
    if (ros_command(ros, routes, 2, route_cb, &ctx) < 0) return -1;

    char chosen_table[128] = {0};
    if (ctx.matched_table_count > 0 && ctx.matched_table_count < 1000 && ctx.matched_table[0]) {
        snprintf(chosen_table, sizeof(chosen_table), "%s", ctx.matched_table);
        printf("Routing table auto-detected: %s\n", chosen_table);
    } else {
        /* Keep first-run UX to one user choice. If there is no unique existing
           table for this tunnel, provision a dedicated Susanin FIB/table and
           default route instead of asking another question. */
        if (ensure_dedicated_table(ros, ctx.selected_if, chosen_table) < 0) return -1;
    }

    if (!ctx.candidates[selected].running) {
        printf("WARNING: selected interface is not currently running. Configuration will still be saved.\n");
    }
    if (config_save_selection(cfg, ctx.selected_if, chosen_table) < 0) return -1;

    printf("\nInstalling/reconciling Susanin data-plane...\n");
    if (install_run(ros, cfg, 0) < 0) {
        fprintf(stderr, "Susanin setup: data-plane installation failed; bootstrap helper retained for retry.\n");
        return -1;
    }

    /* The credentialless bootstrap self-cleans its elevated worker after controller
       start. Keep best-effort legacy helper cleanup here for upgrade safety;
       the restricted agent may not have permission to remove admin-owned
       elevated helpers, so correctness does not depend on these calls. */
    best_effort_remove_named(ros, "/system/scheduler/print",
                             "/system/scheduler/remove",
                             "susanin-bootstrap-worker");
    best_effort_remove_named(ros, "/system/script/print",
                             "/system/script/remove",
                             "susanin-bootstrap-worker");
    best_effort_remove_named(ros, "/system/scheduler/print",
                             "/system/scheduler/remove",
                             "susanin-bootstrap-start");
    best_effort_remove_named(ros, "/system/script/print",
                             "/system/script/remove",
                             "susanin-bootstrap-start");

    printf("\nSusanin setup saved.\n");
    printf("  VPN egress: %s\n", cfg->egress_interface);
    printf("  Routing table: %s\n", cfg->routing_table);
    printf("No username, password, or environment file was requested from the user.\n");
    printf("Data-plane: installed or already in sync.\n");
    return 0;
}
