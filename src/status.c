#include "status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned scripts;
    unsigned schedulers;
    unsigned mangle;
    unsigned tcp_ok;
    unsigned tcp_test;
    unsigned tcp_watch;
    unsigned tcp_cool;
    unsigned udp_ok;
    unsigned udp_test;
    unsigned udp_watch;
    unsigned udp_cool;
} status_ctx_t;

static int script_cb(const ros_sentence_t *s, void *opaque) {
    status_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *disabled = ros_get_attr(s, "disabled");
    if (!name) return 0;
    if (strcmp(name, "auto-awg-health") == 0 || strcmp(name, "auto-awg-fast") == 0 ||
        strcmp(name, "auto-awg-detect") == 0 || strcmp(name, "auto-awg-judge") == 0) {
        printf("  %-18s disabled=%s\n", name, disabled ? disabled : "false");
        ctx->scripts++;
    }
    return 0;
}

static int scheduler_cb(const ros_sentence_t *s, void *opaque) {
    status_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *interval = ros_get_attr(s, "interval");
    const char *disabled = ros_get_attr(s, "disabled");
    const char *run_count = ros_get_attr(s, "run-count");
    if (!name || strncmp(name, "auto-awg-", 9) != 0) return 0;
    printf("  %-18s interval=%-8s disabled=%-5s runs=%s\n",
           name, interval ? interval : "?", disabled ? disabled : "false", run_count ? run_count : "?");
    ctx->schedulers++;
    return 0;
}

static int mangle_cb(const ros_sentence_t *s, void *opaque) {
    status_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *comment = ros_get_attr(s, "comment");
    if (!comment || strncmp(comment, "AUTO-AWG:", 9) != 0) return 0;
    const char *action = ros_get_attr(s, "action");
    const char *proto = ros_get_attr(s, "protocol");
    const char *list = ros_get_attr(s, "dst-address-list");
    const char *disabled = ros_get_attr(s, "disabled");
    printf("  %-38s action=%-15s proto=%-4s list=%-22s disabled=%s\n",
           comment, action ? action : "?", proto ? proto : "-", list ? list : "-", disabled ? disabled : "false");
    ctx->mangle++;
    return 0;
}

static void bump_list(status_ctx_t *ctx, const char *list) {
    if (!list) return;
    if (strcmp(list, "auto_awg_ok_tcp") == 0) ctx->tcp_ok++;
    else if (strcmp(list, "auto_awg_test_tcp") == 0) ctx->tcp_test++;
    else if (strcmp(list, "auto_awg_watch_tcp") == 0) ctx->tcp_watch++;
    else if (strcmp(list, "auto_awg_cooldown_tcp") == 0) ctx->tcp_cool++;
    else if (strcmp(list, "auto_awg_ok_udp") == 0) ctx->udp_ok++;
    else if (strcmp(list, "auto_awg_test_udp") == 0) ctx->udp_test++;
    else if (strcmp(list, "auto_awg_watch_udp") == 0) ctx->udp_watch++;
    else if (strcmp(list, "auto_awg_cooldown_udp") == 0) ctx->udp_cool++;
}

static int addrlist_cb(const ros_sentence_t *s, void *opaque) {
    status_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    bump_list(ctx, ros_get_attr(s, "list"));
    return 0;
}

static int iface_cb(const ros_sentence_t *s, void *opaque) {
    const app_config_t *cfg = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (!name || !cfg->egress_interface || strcmp(name, cfg->egress_interface) != 0) return 0;
    printf("Egress: %s running=%s disabled=%s type=%s\n",
           name,
           ros_get_attr(s, "running") ? ros_get_attr(s, "running") : "?",
           ros_get_attr(s, "disabled") ? ros_get_attr(s, "disabled") : "?",
           ros_get_attr(s, "type") ? ros_get_attr(s, "type") : "?");
    return 0;
}

int status_run(ros_client_t *ros, const app_config_t *cfg) {
    status_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf("=== SUSANIN STATUS ===\n");

    if (cfg->egress_interface) {
        const char *ifs[] = {"/interface/print", "=.proplist=name,type,running,disabled"};
        if (ros_command(ros, ifs, 2, iface_cb, (void *)cfg) < 0) return -1;
    }

    printf("\nScripts:\n");
    const char *scripts[] = {"/system/script/print", "=.proplist=name,disabled"};
    if (ros_command(ros, scripts, 2, script_cb, &ctx) < 0) return -1;

    printf("\nSchedulers:\n");
    const char *sched[] = {"/system/scheduler/print", "=.proplist=name,interval,disabled,run-count"};
    if (ros_command(ros, sched, 2, scheduler_cb, &ctx) < 0) return -1;

    printf("\nMangle rules:\n");
    const char *mangle[] = {"/ip/firewall/mangle/print", "=.proplist=comment,action,protocol,dst-address-list,disabled"};
    if (ros_command(ros, mangle, 2, mangle_cb, &ctx) < 0) return -1;

    const char *alist[] = {"/ip/firewall/address-list/print", "=.proplist=list"};
    if (ros_command(ros, alist, 2, addrlist_cb, &ctx) < 0) return -1;

    printf("\nState cache:\n");
    printf("  TCP ok=%u test=%u watch=%u cooldown=%u\n", ctx.tcp_ok, ctx.tcp_test, ctx.tcp_watch, ctx.tcp_cool);
    printf("  UDP ok=%u test=%u watch=%u cooldown=%u\n", ctx.udp_ok, ctx.udp_test, ctx.udp_watch, ctx.udp_cool);

    printf("\nSummary: scripts=%u/4 schedulers=%u/4 mangle=%u\n",
           ctx.scripts, ctx.schedulers, ctx.mangle);
    if (ctx.scripts == 4 && ctx.schedulers == 4 && ctx.mangle >= 8) {
        printf("Installation state: detected\n");
    } else {
        printf("Installation state: incomplete or not installed\n");
    }
    return 0;
}
