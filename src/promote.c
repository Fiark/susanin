#define _POSIX_C_SOURCE 200809L
#include "promote.h"
#include "fingerprint.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MANAGED_COUNT 4

static const char *prod_names[MANAGED_COUNT] = {
    "auto-awg-health",
    "auto-awg-fast",
    "auto-awg-detect",
    "auto-awg-judge"
};

static const char *stage_names[MANAGED_COUNT] = {
    "susanin-stage-health",
    "susanin-stage-fast",
    "susanin-stage-detect",
    "susanin-stage-judge"
};

static const char *backup_names[MANAGED_COUNT] = {
    "susanin-backup-health",
    "susanin-backup-fast",
    "susanin-backup-detect",
    "susanin-backup-judge"
};

typedef struct {
    char id[64];
    char name[128];
    char *source;
    size_t bytes;
    char fp[17];
    int invalid;
    int found;
} script_obj_t;

typedef struct {
    const char *wanted;
    script_obj_t *out;
} script_lookup_ctx_t;

typedef struct {
    char id[64];
    char name[128];
    char on_event[128];
    int disabled;
    int found;
} scheduler_obj_t;

typedef struct {
    const char *wanted;
    scheduler_obj_t *out;
} scheduler_lookup_ctx_t;

static int attr_true(const char *v) {
    return v && *v && (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0);
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0) { }
}

static void script_obj_free(script_obj_t *o) {
    if (!o) return;
    free(o->source);
    o->source = NULL;
}

static int script_lookup_cb(const ros_sentence_t *s, void *opaque) {
    script_lookup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (!name || strcmp(name, ctx->wanted) != 0) return 0;

    script_obj_t *o = ctx->out;
    const char *id = ros_get_attr(s, ".id");
    const char *source = ros_get_attr(s, "source");
    if (!id || !*id) return 0;

    o->found = 1;
    snprintf(o->id, sizeof(o->id), "%s", id);
    snprintf(o->name, sizeof(o->name), "%s", name);
    o->invalid = attr_true(ros_get_attr(s, "invalid"));
    o->source = strdup(source ? source : "");
    if (!o->source) return -1;
    o->bytes = strlen(o->source);
    susanin_fingerprint_hex(o->source, o->fp);
    return 0;
}

static int script_lookup(ros_client_t *ros, const char *name, script_obj_t *out) {
    memset(out, 0, sizeof(*out));
    script_lookup_ctx_t ctx = {.wanted = name, .out = out};
    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=.id,name,source,invalid,comment"
    };
    return ros_command(ros, cmd, 2, script_lookup_cb, &ctx);
}

static int script_lookup_retry(ros_client_t *ros, const char *name, script_obj_t *out) {
    for (int i = 0; i < 15; ++i) {
        if (script_lookup(ros, name, out) == 0 && out->found) return 0;
        script_obj_free(out);
        sleep_ms(100);
    }
    return -1;
}

static int scheduler_lookup_cb(const ros_sentence_t *s, void *opaque) {
    scheduler_lookup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *name = ros_get_attr(s, "name");
    if (!name || strcmp(name, ctx->wanted) != 0) return 0;
    const char *id = ros_get_attr(s, ".id");
    if (!id || !*id) return 0;

    scheduler_obj_t *o = ctx->out;
    o->found = 1;
    snprintf(o->id, sizeof(o->id), "%s", id);
    snprintf(o->name, sizeof(o->name), "%s", name);
    const char *oe = ros_get_attr(s, "on-event");
    snprintf(o->on_event, sizeof(o->on_event), "%s", oe ? oe : "");
    o->disabled = attr_true(ros_get_attr(s, "disabled"));
    return 0;
}

static int scheduler_lookup(ros_client_t *ros, const char *name, scheduler_obj_t *out) {
    memset(out, 0, sizeof(*out));
    scheduler_lookup_ctx_t ctx = {.wanted = name, .out = out};
    const char *cmd[] = {
        "/system/scheduler/print",
        "=.proplist=.id,name,on-event,disabled,interval"
    };
    return ros_command(ros, cmd, 2, scheduler_lookup_cb, &ctx);
}

static int set_scheduler_disabled(ros_client_t *ros, const char *id, int disabled) {
    char wid[96];
    char wdisabled[32];
    snprintf(wid, sizeof(wid), "=.id=%s", id);
    snprintf(wdisabled, sizeof(wdisabled), "=disabled=%s", disabled ? "true" : "false");
    const char *cmd[] = {"/system/scheduler/set", wid, wdisabled};
    return ros_command(ros, cmd, 3, NULL, NULL);
}

static int set_script_source(ros_client_t *ros, const char *id, const char *source) {
    size_t ilen = strlen(id) + 7;
    size_t slen = strlen(source) + 9;
    char *wid = malloc(ilen);
    char *wsrc = malloc(slen);
    if (!wid || !wsrc) {
        free(wid); free(wsrc);
        return -1;
    }
    snprintf(wid, ilen, "=.id=%s", id);
    snprintf(wsrc, slen, "=source=%s", source);
    const char *cmd[] = {"/system/script/set", wid, wsrc};
    int rc = ros_command(ros, cmd, 3, NULL, NULL);
    free(wid); free(wsrc);
    return rc;
}

static int remove_script_id(ros_client_t *ros, const char *id) {
    char wid[96];
    snprintf(wid, sizeof(wid), "=.id=%s", id);
    const char *cmd[] = {"/system/script/remove", wid};
    return ros_command(ros, cmd, 2, NULL, NULL);
}

static int remove_script_name(ros_client_t *ros, const char *name) {
    script_obj_t o;
    if (script_lookup(ros, name, &o) < 0) return -1;
    if (!o.found) return 0;
    int rc = remove_script_id(ros, o.id);
    script_obj_free(&o);
    return rc;
}

static int add_inert_script(ros_client_t *ros, const char *name, const char *source, const char *comment) {
    size_t nlen = strlen(name) + 7;
    size_t slen = strlen(source) + 9;
    size_t clen = strlen(comment) + 10;
    char *wn = malloc(nlen);
    char *ws = malloc(slen);
    char *wc = malloc(clen);
    if (!wn || !ws || !wc) {
        free(wn); free(ws); free(wc);
        return -1;
    }
    snprintf(wn, nlen, "=name=%s", name);
    snprintf(ws, slen, "=source=%s", source);
    snprintf(wc, clen, "=comment=%s", comment);
    const char *cmd[] = {"/system/script/add", wn, ws, wc};
    int rc = ros_command(ros, cmd, 4, NULL, NULL);
    free(wn); free(ws); free(wc);
    return rc;
}

static int verify_source(ros_client_t *ros, const char *name, const char *fp, size_t bytes) {
    script_obj_t o;
    if (script_lookup_retry(ros, name, &o) < 0) return -1;
    int ok = o.found && !o.invalid && o.bytes == bytes && strcmp(o.fp, fp) == 0;
    script_obj_free(&o);
    return ok ? 0 : -1;
}

static int verify_stage(ros_client_t *ros, const susanin_render_bundle_t *desired) {
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        script_obj_t o;
        if (script_lookup_retry(ros, stage_names[i], &o) < 0) {
            printf("  BLOCK stage missing: %s\n", stage_names[i]);
            return -1;
        }
        int ok = o.found && !o.invalid && o.bytes == desired->scripts[i].bytes &&
                 strcmp(o.fp, desired->scripts[i].fp) == 0;
        if (!ok) {
            printf("  BLOCK stage mismatch: %-22s stage=%zu/%s desired=%zu/%s invalid=%s\n",
                   stage_names[i], o.bytes, o.fp,
                   desired->scripts[i].bytes, desired->scripts[i].fp,
                   o.invalid ? "true" : "false");
            script_obj_free(&o);
            return -1;
        }
        printf("  OK    stage %-22s %zu/%s\n", stage_names[i], o.bytes, o.fp);
        script_obj_free(&o);
    }
    return 0;
}

typedef struct { unsigned active; } jobs_ctx_t;
static int jobs_cb(const ros_sentence_t *s, void *opaque) {
    jobs_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *script = ros_get_attr(s, "script");
    if (!script) return 0;
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (strcmp(script, prod_names[i]) == 0) {
            ctx->active++;
            break;
        }
    }
    return 0;
}

static int wait_jobs_idle(ros_client_t *ros) {
    for (int i = 0; i < 50; ++i) {
        jobs_ctx_t ctx = {0};
        const char *cmd[] = {"/system/script/job/print", "=.proplist=script"};
        if (ros_command(ros, cmd, 2, jobs_cb, &ctx) < 0) return -1;
        if (ctx.active == 0) return 0;
        sleep_ms(100);
    }
    return -1;
}

static void restore_scheduler_states(ros_client_t *ros, scheduler_obj_t sched[MANAGED_COUNT]) {
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (sched[i].found) (void)set_scheduler_disabled(ros, sched[i].id, sched[i].disabled);
    }
}

static int create_backups(ros_client_t *ros, script_obj_t prod[MANAGED_COUNT]) {
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (remove_script_name(ros, backup_names[i]) < 0) return -1;
        char comment[192];
        snprintf(comment, sizeof(comment),
                 "SUSANIN:v0.11.3 rollback backup of %s fp=%s; DO NOT RUN",
                 prod_names[i], prod[i].fp);
        if (add_inert_script(ros, backup_names[i], prod[i].source, comment) < 0) return -1;
        if (verify_source(ros, backup_names[i], prod[i].fp, prod[i].bytes) < 0) return -1;
    }
    return 0;
}

static int restore_sources(ros_client_t *ros, script_obj_t prod[MANAGED_COUNT]) {
    int failed = 0;
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (!prod[i].found || set_script_source(ros, prod[i].id, prod[i].source) < 0 ||
            verify_source(ros, prod_names[i], prod[i].fp, prod[i].bytes) < 0) {
            failed++;
        }
    }
    return failed ? -1 : 0;
}

int promote_dry_run(ros_client_t *ros, const app_config_t *cfg) {
    susanin_render_bundle_t desired;
    if (renderer_build(ros, cfg, &desired) < 0) return -1;
    printf("=== SUSANIN PROMOTE DRY-RUN v0.11.3 ===\n");
    printf("RouterOS changes: NONE\n\n");
    int rc = verify_stage(ros, &desired);
    if (rc == 0) {
        printf("\nSafety gates: PASS\n");
        printf("Would: snapshot production -> create rollback backups -> pause schedulers -> wait jobs idle -> update 4 sources -> verify -> resume.\n");
    } else {
        printf("\nSafety gates: BLOCKED\n");
    }
    renderer_free(&desired);
    return rc;
}

int promote_run(ros_client_t *ros, const app_config_t *cfg) {
    susanin_render_bundle_t desired;
    if (renderer_build(ros, cfg, &desired) < 0) return -1;

    printf("=== SUSANIN PROMOTE v0.11.3 ===\n");
    printf("Transactional source promotion with automatic rollback on failure.\n\n");

    if (verify_stage(ros, &desired) < 0) {
        printf("Promotion blocked: stage is absent or does not match generated desired source.\n");
        renderer_free(&desired);
        return -1;
    }

    script_obj_t prod[MANAGED_COUNT];
    scheduler_obj_t sched[MANAGED_COUNT];
    memset(prod, 0, sizeof(prod));
    memset(sched, 0, sizeof(sched));

    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (script_lookup(ros, prod_names[i], &prod[i]) < 0 || !prod[i].found) {
            printf("BLOCK production script missing: %s\n", prod_names[i]);
            goto fail_pre;
        }
        if (scheduler_lookup(ros, prod_names[i], &sched[i]) < 0 || !sched[i].found) {
            printf("BLOCK scheduler missing: %s\n", prod_names[i]);
            goto fail_pre;
        }
        if (strcmp(sched[i].on_event, prod_names[i]) != 0) {
            printf("BLOCK scheduler %s on-event=%s expected=%s\n",
                   sched[i].name, sched[i].on_event, prod_names[i]);
            goto fail_pre;
        }
    }

    printf("Creating persistent rollback backups...\n");
    if (create_backups(ros, prod) < 0) {
        printf("BLOCK could not create/verify rollback backups. Production unchanged.\n");
        goto fail_pre;
    }

    printf("Pausing managed schedulers...\n");
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (set_scheduler_disabled(ros, sched[i].id, 1) < 0) {
            printf("FAIL could not disable scheduler %s\n", sched[i].name);
            restore_scheduler_states(ros, sched);
            goto fail_pre;
        }
    }

    if (wait_jobs_idle(ros) < 0) {
        printf("FAIL managed script jobs did not become idle; restoring scheduler states.\n");
        restore_scheduler_states(ros, sched);
        goto fail_pre;
    }

    printf("Promoting generated sources...\n");
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (set_script_source(ros, prod[i].id, desired.scripts[i].source) < 0 ||
            verify_source(ros, prod_names[i], desired.scripts[i].fp, desired.scripts[i].bytes) < 0) {
            printf("FAIL %-18s update/read-back mismatch; rolling back all production sources.\n", prod_names[i]);
            int rb = restore_sources(ros, prod);
            restore_scheduler_states(ros, sched);
            printf("Rollback result: %s\n", rb == 0 ? "SUCCESS" : "FAILED - MANUAL RESTORE REQUIRED");
            goto fail_pre;
        }
        printf("  PROMOTE %-18s %zu/%s\n",
               prod_names[i], desired.scripts[i].bytes, desired.scripts[i].fp);
    }

    restore_scheduler_states(ros, sched);

    printf("Cleaning stage objects after successful promotion...\n");
    for (size_t i = 0; i < MANAGED_COUNT; ++i) (void)remove_script_name(ros, stage_names[i]);

    printf("\nPromotion result: SUCCESS\n");
    printf("Rollback backups retained: YES (susanin-backup-*)\n");
    printf("Scheduler states restored: YES\n");

    for (size_t i = 0; i < MANAGED_COUNT; ++i) script_obj_free(&prod[i]);
    renderer_free(&desired);
    return 0;

fail_pre:
    for (size_t i = 0; i < MANAGED_COUNT; ++i) script_obj_free(&prod[i]);
    renderer_free(&desired);
    return -1;
}

int rollback_run(ros_client_t *ros) {
    printf("=== SUSANIN ROLLBACK v0.11.3 ===\n");
    script_obj_t prod[MANAGED_COUNT];
    script_obj_t backup[MANAGED_COUNT];
    scheduler_obj_t sched[MANAGED_COUNT];
    memset(prod, 0, sizeof(prod));
    memset(backup, 0, sizeof(backup));
    memset(sched, 0, sizeof(sched));

    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (script_lookup(ros, prod_names[i], &prod[i]) < 0 || !prod[i].found ||
            script_lookup(ros, backup_names[i], &backup[i]) < 0 || !backup[i].found || backup[i].invalid ||
            scheduler_lookup(ros, prod_names[i], &sched[i]) < 0 || !sched[i].found) {
            printf("BLOCK rollback prerequisite missing for %s\n", prod_names[i]);
            goto fail;
        }
    }

    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (set_scheduler_disabled(ros, sched[i].id, 1) < 0) goto fail_restore_sched;
    }
    if (wait_jobs_idle(ros) < 0) goto fail_restore_sched;

    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        if (set_script_source(ros, prod[i].id, backup[i].source) < 0 ||
            verify_source(ros, prod_names[i], backup[i].fp, backup[i].bytes) < 0) {
            printf("FAIL rollback source restore for %s\n", prod_names[i]);
            goto fail_restore_sched;
        }
        printf("  RESTORE %-18s %zu/%s\n", prod_names[i], backup[i].bytes, backup[i].fp);
    }

    restore_scheduler_states(ros, sched);
    printf("Rollback result: SUCCESS\n");
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        script_obj_free(&prod[i]);
        script_obj_free(&backup[i]);
    }
    return 0;

fail_restore_sched:
    restore_scheduler_states(ros, sched);
fail:
    for (size_t i = 0; i < MANAGED_COUNT; ++i) {
        script_obj_free(&prod[i]);
        script_obj_free(&backup[i]);
    }
    return -1;
}
