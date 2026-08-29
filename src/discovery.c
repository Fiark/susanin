#include "discovery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LAN_IFS 64

typedef struct {
    const app_config_t *cfg;
    char lan_ifs[MAX_LAN_IFS][128];
    size_t lan_if_count;
    int egress_found;
    int egress_running;
    int table_found;
} discovery_ctx_t;

static int has_lan_if(const discovery_ctx_t *ctx, const char *name) {
    for (size_t i = 0; i < ctx->lan_if_count; ++i) {
        if (strcmp(ctx->lan_ifs[i], name) == 0) return 1;
    }
    return 0;
}

static int resource_cb(const ros_sentence_t *s, void *opaque) {
    (void)opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *version = ros_get_attr(s, "version");
    const char *board = ros_get_attr(s, "board-name");
    const char *arch = ros_get_attr(s, "architecture-name");
    const char *cpu = ros_get_attr(s, "cpu");
    printf("RouterOS: %s\n", version ? version : "?");
    printf("Board: %s\n", board ? board : "?");
    printf("Architecture: %s\n", arch ? arch : "?");
    printf("CPU: %s\n", cpu ? cpu : "?");
    return 0;
}

static int lan_member_cb(const ros_sentence_t *s, void *opaque) {
    discovery_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *list = ros_get_attr(s, "list");
    const char *iface = ros_get_attr(s, "interface");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!list || strcmp(list, ctx->cfg->lan_list) != 0) return 0;
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "yes") == 0)) return 0;
    if (!iface || !*iface) return 0;
    if (ctx->lan_if_count < MAX_LAN_IFS && !has_lan_if(ctx, iface)) {
        snprintf(ctx->lan_ifs[ctx->lan_if_count++], sizeof(ctx->lan_ifs[0]), "%s", iface);
    }
    return 0;
}

static int address_cb(const ros_sentence_t *s, void *opaque) {
    discovery_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *iface = ros_get_attr(s, "interface");
    const char *addr = ros_get_attr(s, "address");
    const char *network = ros_get_attr(s, "network");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!iface || !addr || !has_lan_if(ctx, iface)) return 0;
    if (disabled && strcmp(disabled, "yes") == 0) return 0;
    printf("  %-20s %-20s network=%s\n", iface, addr, network ? network : "?");
    return 0;
}

static int interface_cb(const ros_sentence_t *s, void *opaque) {
    discovery_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *type = ros_get_attr(s, "type");
    const char *running = ros_get_attr(s, "running");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!name) return 0;

    if (ctx->cfg->egress_interface && strcmp(name, ctx->cfg->egress_interface) == 0) {
        ctx->egress_found = 1;
        ctx->egress_running = running && strcmp(running, "true") == 0;
    }

    if (type && (strcmp(type, "wg") == 0 || strcmp(type, "ovpn-out") == 0 ||
                 strstr(name, "wg") || strstr(name, "vpn") || strstr(name, "tun"))) {
        printf("  %-24s type=%-12s running=%-5s disabled=%s\n",
               name, type, running ? running : "?", disabled ? disabled : "?");
    }
    return 0;
}

static int table_cb(const ros_sentence_t *s, void *opaque) {
    discovery_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *fib = ros_get_attr(s, "fib");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!name) return 0;
    printf("  %-24s fib=%-5s disabled=%s\n", name, fib ? fib : "?", disabled ? disabled : "?");
    if (ctx->cfg->routing_table && strcmp(name, ctx->cfg->routing_table) == 0) ctx->table_found = 1;
    return 0;
}

int discovery_run(ros_client_t *ros, const app_config_t *cfg, int plan_mode) {
    discovery_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;

    const char *resource[] = {"/system/resource/print", "=.proplist=version,board-name,architecture-name,cpu"};
    if (ros_command(ros, resource, 2, resource_cb, &ctx) < 0) return -1;

    printf("\nLAN interface-list '%s' members:\n", cfg->lan_list);
    const char *members[] = {"/interface/list/member/print", "=.proplist=list,interface,disabled"};
    if (ros_command(ros, members, 2, lan_member_cb, &ctx) < 0) return -1;
    if (ctx.lan_if_count == 0) printf("  <none>\n");
    for (size_t i = 0; i < ctx.lan_if_count; ++i) printf("  %s\n", ctx.lan_ifs[i]);

    printf("\nLAN addresses:\n");
    const char *addrs[] = {"/ip/address/print", "=.proplist=address,network,interface,disabled"};
    if (ros_command(ros, addrs, 2, address_cb, &ctx) < 0) return -1;

    printf("\nPotential tunnel/egress interfaces:\n");
    const char *ifs[] = {"/interface/print", "=.proplist=name,type,running,disabled"};
    if (ros_command(ros, ifs, 2, interface_cb, &ctx) < 0) return -1;

    printf("\nRouting tables:\n");
    const char *tables[] = {"/routing/table/print", "=.proplist=name,fib,disabled"};
    if (ros_command(ros, tables, 2, table_cb, &ctx) < 0) return -1;

    if (plan_mode) {
        printf("\n=== PLAN ===\n");
        printf("LAN source: interface-list=%s (%zu member%s)\n",
               cfg->lan_list, ctx.lan_if_count, ctx.lan_if_count == 1 ? "" : "s");
        if (cfg->egress_interface) {
            printf("Egress interface: %s [%s%s]\n", cfg->egress_interface,
                   ctx.egress_found ? "found" : "NOT FOUND",
                   ctx.egress_found ? (ctx.egress_running ? ", running" : ", not running") : "");
        } else {
            printf("Egress interface: not selected yet\n");
        }
        if (cfg->routing_table) {
            printf("Routing table: %s [%s]\n", cfg->routing_table,
                   ctx.table_found ? "found" : "NOT FOUND");
        } else {
            printf("Routing table: not selected yet\n");
        }
        printf("Planned RouterOS data-plane objects: FAST, SOFT, JUDGE, HEALTH, TCP/UDP state lists, mangle marks.\n");
        printf("No changes were made.\n");
    }
    return 0;
}
