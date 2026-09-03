#define _POSIX_C_SOURCE 200809L
#include "install.h"
#include "fingerprint.h"
#include "renderer.h"
#include "validate.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MANAGED_SCRIPTS 4
#define MANAGED_SCHEDULERS 4
#define MANAGED_MANGLE 8
#define SAFETY_MANGLE 3

typedef struct {
    char id[64];
    int found;
    int disabled;
    int invalid;
    char source_fp[17];
    size_t source_bytes;
    char interval[32];
    char on_event[128];
} object_info_t;

typedef struct {
    const char *name;
    const char *interval;
} scheduler_spec_t;

typedef struct {
    const char *comment;
    const char *chain;
    const char *action;
    const char *protocol;
    const char *dst_address;
    const char *dst_list;
    const char *conn_mark;
    const char *new_conn_mark;
    const char *dst_port;
    int passthrough; /* -1 unset, 0 no, 1 yes */
    int connection_new;
    int dst_not_local;
    int use_lan_list;
    int use_route_mark;
} mangle_spec_t;

static const char *script_names[MANAGED_SCRIPTS] = {
    "auto-awg-health", "auto-awg-fast", "auto-awg-detect", "auto-awg-judge"
};

static const scheduler_spec_t scheduler_specs[MANAGED_SCHEDULERS] = {
    {"auto-awg-health", "3s"},
    {"auto-awg-fast", "1s"},
    {"auto-awg-detect", "2s"},
    {"auto-awg-judge", "1s"}
};

static const mangle_spec_t safety_specs[SAFETY_MANGLE] = {
    {"SUSANIN: safety bypass private 10.0.0.0/8", "prerouting", "accept", NULL, "10.0.0.0/8", NULL, NULL, NULL, NULL, -1, 0, 0, 1, 0},
    {"SUSANIN: safety bypass private 172.16.0.0/12", "prerouting", "accept", NULL, "172.16.0.0/12", NULL, NULL, NULL, NULL, -1, 0, 0, 1, 0},
    {"SUSANIN: safety bypass private 192.168.0.0/16", "prerouting", "accept", NULL, "192.168.0.0/16", NULL, NULL, NULL, NULL, -1, 0, 0, 1, 0}
};

static const mangle_spec_t mangle_specs[MANAGED_MANGLE] = {
    {"AUTO-AWG: L2 mark confirmed", "prerouting", "mark-connection", "tcp", NULL, "auto_awg_ok_tcp", "no-mark", "auto-awg-ok-conn", NULL, 1, 1, 1, 1, 0},
    {"AUTO-AWG: L1 mark test", "prerouting", "mark-connection", "tcp", NULL, "auto_awg_test_tcp", "no-mark", "auto-awg-test-conn", NULL, 1, 1, 1, 1, 0},
    {"AUTO-AWG: L2 mark confirmed UDP", "prerouting", "mark-connection", "udp", NULL, "auto_awg_ok_udp", "no-mark", "auto-awg-ok-conn", NULL, 1, 1, 1, 1, 0},
    {"AUTO-AWG: L1 mark test UDP", "prerouting", "mark-connection", "udp", NULL, "auto_awg_test_udp", "no-mark", "auto-awg-test-conn", NULL, 1, 1, 1, 1, 0},
    {"AUTO-AWG: L2 route confirmed", "prerouting", "mark-routing", NULL, NULL, NULL, "auto-awg-ok-conn", NULL, NULL, 0, 0, 0, 1, 1},
    {"AUTO-AWG: L1 route test", "prerouting", "mark-routing", NULL, NULL, NULL, "auto-awg-test-conn", NULL, NULL, 0, 0, 0, 1, 1},
    {"AUTO-AWG: router DNS UDP via tunnel", "output", "mark-routing", "udp", NULL, NULL, NULL, NULL, "53", 0, 0, 0, 0, 1},
    {"AUTO-AWG: router DNS TCP via tunnel", "output", "mark-routing", "tcp", NULL, NULL, NULL, NULL, "53", 0, 0, 0, 0, 1}
};

static int attr_true(const char *v) {
    return v && *v && (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0);
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0) { }
}

typedef struct {
    const char *wanted_name;
    const char *wanted_comment;
    object_info_t *out;
} lookup_ctx_t;

static int lookup_cb(const ros_sentence_t *s, void *opaque) {
    lookup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    const char *comment = ros_get_attr(s, "comment");
    if (ctx->wanted_name && (!name || strcmp(name, ctx->wanted_name) != 0)) return 0;
    if (ctx->wanted_comment && (!comment || strcmp(comment, ctx->wanted_comment) != 0)) return 0;
    object_info_t *o = ctx->out;
    const char *id = ros_get_attr(s, ".id");
    if (!id || !*id) return 0;
    o->found = 1;
    snprintf(o->id, sizeof(o->id), "%s", id);
    o->disabled = attr_true(ros_get_attr(s, "disabled"));
    o->invalid = attr_true(ros_get_attr(s, "invalid"));
    const char *source = ros_get_attr(s, "source");
    if (source) {
        o->source_bytes = strlen(source);
        susanin_fingerprint_hex(source, o->source_fp);
    }
    const char *interval = ros_get_attr(s, "interval");
    const char *event = ros_get_attr(s, "on-event");
    if (interval) snprintf(o->interval, sizeof(o->interval), "%s", interval);
    if (event) snprintf(o->on_event, sizeof(o->on_event), "%s", event);
    return 0;
}

static int lookup_name(ros_client_t *ros, const char *print_cmd, const char *name,
                       const char *proplist, object_info_t *out) {
    memset(out, 0, sizeof(*out));
    char prop[512];
    char query[256];
    snprintf(prop, sizeof(prop), "=.proplist=%s", proplist);
    snprintf(query, sizeof(query), "?name=%s", name);
    lookup_ctx_t ctx = {.wanted_name = name, .wanted_comment = NULL, .out = out};
    const char *cmd[] = {print_cmd, prop, query};
    return ros_command(ros, cmd, 3, lookup_cb, &ctx);
}

static int lookup_comment(ros_client_t *ros, const char *print_cmd, const char *comment,
                          const char *proplist, object_info_t *out) {
    memset(out, 0, sizeof(*out));
    char prop[512];
    char query[320];
    snprintf(prop, sizeof(prop), "=.proplist=%s", proplist);
    snprintf(query, sizeof(query), "?comment=%s", comment);
    lookup_ctx_t ctx = {.wanted_name = NULL, .wanted_comment = comment, .out = out};
    const char *cmd[] = {print_cmd, prop, query};
    return ros_command(ros, cmd, 3, lookup_cb, &ctx);
}

static int remove_id(ros_client_t *ros, const char *remove_cmd, const char *id) {
    char wid[96];
    snprintf(wid, sizeof(wid), "=.id=%s", id);
    const char *cmd[] = {remove_cmd, wid};
    return ros_command(ros, cmd, 2, NULL, NULL);
}

static int set_disabled_id(ros_client_t *ros, const char *set_cmd, const char *id, int disabled) {
    char wid[96], wdisabled[32];
    snprintf(wid, sizeof(wid), "=.id=%s", id);
    snprintf(wdisabled, sizeof(wdisabled), "=disabled=%s", disabled ? "yes" : "no");
    const char *cmd[] = {set_cmd, wid, wdisabled};
    return ros_command(ros, cmd, 3, NULL, NULL);
}

static int add_script(ros_client_t *ros, const susanin_rendered_script_t *s) {
    size_t n1 = strlen(s->name) + 7;
    size_t n2 = strlen(s->source) + 9;
    char comment[192];
    snprintf(comment, sizeof(comment), "SUSANIN:v" SUSANIN_VERSION " managed source fp=%s", s->fp);
    size_t n3 = strlen(comment) + 10;
    char *wname = malloc(n1), *wsource = malloc(n2), *wcomment = malloc(n3);
    if (!wname || !wsource || !wcomment) {
        free(wname); free(wsource); free(wcomment); return -1;
    }
    snprintf(wname, n1, "=name=%s", s->name);
    snprintf(wsource, n2, "=source=%s", s->source);
    snprintf(wcomment, n3, "=comment=%s", comment);
    const char *cmd[] = {"/system/script/add", wname, wsource, wcomment};
    int rc = ros_command(ros, cmd, 4, NULL, NULL);
    free(wname); free(wsource); free(wcomment);
    return rc;
}

static int verify_script(ros_client_t *ros, const susanin_rendered_script_t *s) {
    for (int i = 0; i < 15; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/script/print", s->name,
                        ".id,name,source,invalid", &o) == 0 && o.found) {
            return (!o.invalid && o.source_bytes == s->bytes && strcmp(o.source_fp, s->fp) == 0) ? 0 : -1;
        }
        sleep_ms(100);
    }
    return -1;
}

static int add_scheduler(ros_client_t *ros, const scheduler_spec_t *s) {
    char wname[160], winterval[64], wevent[160];
    snprintf(wname, sizeof(wname), "=name=%s", s->name);
    snprintf(winterval, sizeof(winterval), "=interval=%s", s->interval);
    snprintf(wevent, sizeof(wevent), "=on-event=%s", s->name);
    const char *cmd[] = {
        "/system/scheduler/add", wname, "=start-time=startup", winterval, wevent,
        "=disabled=yes", "=policy=read,write,test", "=comment=SUSANIN: managed scheduler"
    };
    return ros_command(ros, cmd, 8, NULL, NULL);
}

static int verify_scheduler(ros_client_t *ros, const scheduler_spec_t *s, int expect_disabled) {
    object_info_t o;
    if (lookup_name(ros, "/system/scheduler/print", s->name,
                    ".id,name,interval,on-event,disabled", &o) < 0 || !o.found) return -1;
    return strcmp(o.interval, s->interval) == 0 && strcmp(o.on_event, s->name) == 0 &&
           o.disabled == expect_disabled ? 0 : -1;
}

static int add_mangle(ros_client_t *ros, const mangle_spec_t *s, const app_config_t *cfg) {
    char wcomment[256], wchain[64], waction[64], wproto[64], wdstaddr[96], wdstlist[128];
    char wconn[128], wnewconn[128], wport[64], wlan[192], wroute[192];
    const char *cmd[20];
    size_t n = 0;
    cmd[n++] = "/ip/firewall/mangle/add";
    snprintf(wchain, sizeof(wchain), "=chain=%s", s->chain); cmd[n++] = wchain;
    snprintf(waction, sizeof(waction), "=action=%s", s->action); cmd[n++] = waction;
    snprintf(wcomment, sizeof(wcomment), "=comment=%s", s->comment); cmd[n++] = wcomment;
    cmd[n++] = "=disabled=yes";
    if (s->protocol) { snprintf(wproto, sizeof(wproto), "=protocol=%s", s->protocol); cmd[n++] = wproto; }
    if (s->dst_address) { snprintf(wdstaddr, sizeof(wdstaddr), "=dst-address=%s", s->dst_address); cmd[n++] = wdstaddr; }
    if (s->dst_list) { snprintf(wdstlist, sizeof(wdstlist), "=dst-address-list=%s", s->dst_list); cmd[n++] = wdstlist; }
    if (s->conn_mark) { snprintf(wconn, sizeof(wconn), "=connection-mark=%s", s->conn_mark); cmd[n++] = wconn; }
    if (s->new_conn_mark) { snprintf(wnewconn, sizeof(wnewconn), "=new-connection-mark=%s", s->new_conn_mark); cmd[n++] = wnewconn; }
    if (s->dst_port) { snprintf(wport, sizeof(wport), "=dst-port=%s", s->dst_port); cmd[n++] = wport; }
    if (s->passthrough >= 0) cmd[n++] = s->passthrough ? "=passthrough=yes" : "=passthrough=no";
    if (s->connection_new) cmd[n++] = "=connection-state=new";
    if (s->dst_not_local) cmd[n++] = "=dst-address-type=!local";
    if (s->use_lan_list) { snprintf(wlan, sizeof(wlan), "=in-interface-list=%s", cfg->lan_list); cmd[n++] = wlan; }
    if (s->use_route_mark) { snprintf(wroute, sizeof(wroute), "=new-routing-mark=%s", cfg->routing_table); cmd[n++] = wroute; }
    return ros_command(ros, cmd, n, NULL, NULL);
}

static int verify_comment_object(ros_client_t *ros, const char *print_cmd, const char *comment,
                                 int expect_disabled, object_info_t *out) {
    if (lookup_comment(ros, print_cmd, comment, ".id,comment,disabled", out) < 0 || !out->found) return -1;
    return out->disabled == expect_disabled ? 0 : -1;
}

typedef struct {
    const char *egress;
    int found_active;
} nat_scan_ctx_t;

static int nat_scan_cb(const ros_sentence_t *s, void *opaque) {
    nat_scan_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *chain = ros_get_attr(s, "chain");
    const char *action = ros_get_attr(s, "action");
    const char *out = ros_get_attr(s, "out-interface");
    const char *disabled = ros_get_attr(s, "disabled");
    if (chain && action && out && strcmp(chain, "srcnat") == 0 &&
        strcmp(action, "masquerade") == 0 && strcmp(out, ctx->egress) == 0 && !attr_true(disabled)) {
        ctx->found_active = 1;
    }
    return 0;
}

static int has_existing_tunnel_nat(ros_client_t *ros, const app_config_t *cfg) {
    nat_scan_ctx_t ctx = {.egress = cfg->egress_interface, .found_active = 0};
    const char *cmd[] = {"/ip/firewall/nat/print", "=.proplist=chain,action,out-interface,disabled"};
    if (ros_command(ros, cmd, 2, nat_scan_cb, &ctx) < 0) return -1;
    return ctx.found_active;
}

static int add_tunnel_nat(ros_client_t *ros, const app_config_t *cfg) {
    char wout[192];
    snprintf(wout, sizeof(wout), "=out-interface=%s", cfg->egress_interface);
    const char *cmd[] = {
        "/ip/firewall/nat/add", "=chain=srcnat", "=action=masquerade", wout,
        "=disabled=yes", "=comment=SUSANIN: masquerade selected tunnel"
    };
    return ros_command(ros, cmd, 6, NULL, NULL);
}

static int count_present(ros_client_t *ros, unsigned *out_present, unsigned *out_support) {
    unsigned present = 0;
    unsigned support = 0;
    if (!out_present || !out_support) return -1;
    for (size_t i = 0; i < MANAGED_SCRIPTS; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/script/print", script_names[i], ".id,name", &o) < 0) return -1;
        if (o.found) present++;
    }
    for (size_t i = 0; i < MANAGED_SCHEDULERS; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/scheduler/print", scheduler_specs[i].name, ".id,name", &o) < 0) return -1;
        if (o.found) present++;
    }
    for (size_t i = 0; i < MANAGED_MANGLE; ++i) {
        object_info_t o;
        if (lookup_comment(ros, "/ip/firewall/mangle/print", mangle_specs[i].comment, ".id,comment", &o) < 0) return -1;
        if (o.found) present++;
    }
    for (size_t i = 0; i < SAFETY_MANGLE; ++i) {
        object_info_t o;
        if (lookup_comment(ros, "/ip/firewall/mangle/print", safety_specs[i].comment, ".id,comment", &o) < 0) return -1;
        if (o.found) support++;
    }
    object_info_t nat;
    if (lookup_comment(ros, "/ip/firewall/nat/print", "SUSANIN: masquerade selected tunnel", ".id,comment", &nat) < 0) return -1;
    if (nat.found) support++;
    *out_present = present;
    *out_support = support;
    return 0;
}

static void rollback_fresh(ros_client_t *ros, int created_nat) {
    /* Schedulers first so no managed script can run while we dismantle. */
    for (size_t i = 0; i < MANAGED_SCHEDULERS; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/scheduler/print", scheduler_specs[i].name, ".id,name", &o) == 0 && o.found)
            (void)remove_id(ros, "/system/scheduler/remove", o.id);
    }
    for (size_t i = 0; i < MANAGED_MANGLE; ++i) {
        object_info_t o;
        if (lookup_comment(ros, "/ip/firewall/mangle/print", mangle_specs[i].comment, ".id,comment", &o) == 0 && o.found)
            (void)remove_id(ros, "/ip/firewall/mangle/remove", o.id);
    }
    for (size_t i = 0; i < SAFETY_MANGLE; ++i) {
        object_info_t o;
        if (lookup_comment(ros, "/ip/firewall/mangle/print", safety_specs[i].comment, ".id,comment", &o) == 0 && o.found)
            (void)remove_id(ros, "/ip/firewall/mangle/remove", o.id);
    }
    if (created_nat) {
        object_info_t o;
        if (lookup_comment(ros, "/ip/firewall/nat/print", "SUSANIN: masquerade selected tunnel", ".id,comment", &o) == 0 && o.found)
            (void)remove_id(ros, "/ip/firewall/nat/remove", o.id);
    }
    for (size_t i = 0; i < MANAGED_SCRIPTS; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/script/print", script_names[i], ".id,name", &o) == 0 && o.found)
            (void)remove_id(ros, "/system/script/remove", o.id);
    }
}

static int enable_comment(ros_client_t *ros, const char *menu, const char *set_cmd, const char *comment) {
    object_info_t o;
    if (lookup_comment(ros, menu, comment, ".id,comment,disabled", &o) < 0 || !o.found) return -1;
    return set_disabled_id(ros, set_cmd, o.id, 0);
}

int install_run(ros_client_t *ros, const app_config_t *cfg, int dry_run) {
    if (!ros || !cfg || !cfg->egress_interface || !cfg->routing_table || !cfg->lan_list) {
        fprintf(stderr, "Susanin install: setup selection is incomplete.\n");
        return -1;
    }

    unsigned present = 0, support = 0;
    if (count_present(ros, &present, &support) < 0) {
        fprintf(stderr, "Susanin install: failed to inspect current RouterOS managed objects.\n");
        return -1;
    }
    if (dry_run) {
        printf("=== SUSANIN FRESH INSTALL DRY-RUN v%s ===\n", SUSANIN_VERSION);
        printf("RouterOS changes: NONE\n");
        printf("Managed legacy-compatible objects present: %u/16\n", present);
        printf("Susanin fresh-install support objects present: %u/4\n", support);
        if (present == 16) {
            printf("Result: EXISTING INSTALLATION DETECTED; fresh install not required.\n");
            return 0;
        }
        if (present != 0 || support != 0) {
            printf("Result: BLOCKED - partial managed/fresh-install support state detected.\n");
            return -1;
        }
        susanin_render_bundle_t desired;
        if (renderer_build(ros, cfg, &desired) < 0) return -1;
        printf("Would create transactionally:\n");
        printf("  4 generated scripts (validated before commit)\n");
        printf("  4 schedulers (created disabled, enabled last)\n");
        printf("  8 AUTO-AWG compatibility mangle rules\n");
        printf("  3 SUSANIN private-network safety bypass rules\n");
        printf("  tunnel masquerade only if no active masquerade exists for %s\n", cfg->egress_interface);
        printf("Desired script fingerprints:\n");
        for (size_t i = 0; i < MANAGED_SCRIPTS; ++i)
            printf("  %-18s %zu/%s\n", desired.scripts[i].name, desired.scripts[i].bytes, desired.scripts[i].fp);
        renderer_free(&desired);
        printf("Result: READY FOR FRESH INSTALL.\n");
        return 0;
    }

    printf("=== SUSANIN FRESH INSTALL v%s ===\n", SUSANIN_VERSION);
    if (present == 16) {
        printf("Existing complete installation detected. Data-plane changes: NONE.\n");
        return 0;
    }
    if (present != 0 || support != 0) {
        printf("BLOCKED: partial managed installation detected (%u/16 managed, %u/4 support objects).\n", present, support);
        printf("Susanin will not guess which partial objects are safe to replace.\n");
        return -1;
    }

    susanin_render_bundle_t desired;
    if (renderer_build(ros, cfg, &desired) < 0) return -1;

    printf("Preflight: validating generated RouterOS source...\n");
    if (validate_run(ros, cfg) < 0) {
        printf("Fresh install blocked: generated source validation failed.\n");
        renderer_free(&desired);
        return -1;
    }

    int created_nat = 0;
    printf("Creating production scripts...\n");
    for (size_t i = 0; i < MANAGED_SCRIPTS; ++i) {
        if (add_script(ros, &desired.scripts[i]) < 0 || verify_script(ros, &desired.scripts[i]) < 0) {
            printf("FAIL script %s; rolling back fresh install.\n", desired.scripts[i].name);
            rollback_fresh(ros, created_nat);
            renderer_free(&desired);
            return -1;
        }
        printf("  [OK] %-18s %zu/%s\n", desired.scripts[i].name, desired.scripts[i].bytes, desired.scripts[i].fp);
    }

    printf("Creating safety and routing mangle rules disabled...\n");
    for (size_t i = 0; i < SAFETY_MANGLE; ++i) {
        if (add_mangle(ros, &safety_specs[i], cfg) < 0) {
            printf("FAIL safety mangle; rolling back fresh install.\n");
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
        object_info_t o;
        if (verify_comment_object(ros, "/ip/firewall/mangle/print", safety_specs[i].comment, 1, &o) < 0) {
            printf("FAIL safety mangle read-back; rolling back fresh install.\n");
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
    }
    for (size_t i = 0; i < MANAGED_MANGLE; ++i) {
        if (add_mangle(ros, &mangle_specs[i], cfg) < 0) {
            printf("FAIL mangle %s; rolling back fresh install.\n", mangle_specs[i].comment);
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
        object_info_t o;
        if (verify_comment_object(ros, "/ip/firewall/mangle/print", mangle_specs[i].comment, 1, &o) < 0) {
            printf("FAIL mangle read-back %s; rolling back fresh install.\n", mangle_specs[i].comment);
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
    }

    int existing_nat = has_existing_tunnel_nat(ros, cfg);
    if (existing_nat < 0) {
        rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
    }
    if (!existing_nat) {
        if (add_tunnel_nat(ros, cfg) < 0) {
            printf("FAIL tunnel NAT; rolling back fresh install.\n");
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
        object_info_t o;
        if (verify_comment_object(ros, "/ip/firewall/nat/print", "SUSANIN: masquerade selected tunnel", 1, &o) < 0) {
            printf("FAIL tunnel NAT read-back; rolling back fresh install.\n");
            rollback_fresh(ros, 1); renderer_free(&desired); return -1;
        }
        created_nat = 1;
        printf("  [OK] tunnel masquerade prepared\n");
    } else {
        printf("  [KEEP] existing active masquerade for %s\n", cfg->egress_interface);
    }

    printf("Creating schedulers disabled...\n");
    for (size_t i = 0; i < MANAGED_SCHEDULERS; ++i) {
        if (add_scheduler(ros, &scheduler_specs[i]) < 0 || verify_scheduler(ros, &scheduler_specs[i], 1) < 0) {
            printf("FAIL scheduler %s; rolling back fresh install.\n", scheduler_specs[i].name);
            rollback_fresh(ros, created_nat); renderer_free(&desired); return -1;
        }
        printf("  [OK] %-18s interval=%s\n", scheduler_specs[i].name, scheduler_specs[i].interval);
    }

    printf("Committing data-plane...\n");
    for (size_t i = 0; i < SAFETY_MANGLE; ++i) {
        if (enable_comment(ros, "/ip/firewall/mangle/print", "/ip/firewall/mangle/set", safety_specs[i].comment) < 0) goto commit_fail;
    }
    for (size_t i = 0; i < MANAGED_MANGLE; ++i) {
        if (enable_comment(ros, "/ip/firewall/mangle/print", "/ip/firewall/mangle/set", mangle_specs[i].comment) < 0) goto commit_fail;
    }
    if (created_nat && enable_comment(ros, "/ip/firewall/nat/print", "/ip/firewall/nat/set", "SUSANIN: masquerade selected tunnel") < 0) goto commit_fail;
    for (size_t i = 0; i < MANAGED_SCHEDULERS; ++i) {
        object_info_t o;
        if (lookup_name(ros, "/system/scheduler/print", scheduler_specs[i].name, ".id,name,disabled", &o) < 0 || !o.found ||
            set_disabled_id(ros, "/system/scheduler/set", o.id, 0) < 0) goto commit_fail;
    }

    printf("\nFresh install result: SUCCESS\n");
    printf("  scripts=4 schedulers=4 mangle=8 safety=3\n");
    printf("  tunnel NAT=%s\n", created_nat ? "CREATED" : "EXISTING");
    printf("  data-plane started\n");
    renderer_free(&desired);
    return 0;

commit_fail:
    printf("FAIL during commit; rolling back all objects created by this fresh install.\n");
    rollback_fresh(ros, created_nat);
    renderer_free(&desired);
    return -1;
}
