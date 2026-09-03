#define _POSIX_C_SOURCE 200809L
#include "stage.h"
#include "fingerprint.h"
#include "renderer.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STAGE_COUNT 4

typedef struct {
    char id[64];
    char name[128];
    int found;
    int invalid;
    size_t bytes;
    char fp[17];
} stage_obj_t;

typedef struct {
    const char *wanted_name;
    stage_obj_t *out;
} lookup_ctx_t;

static const char *stage_names[STAGE_COUNT] = {
    "susanin-stage-health",
    "susanin-stage-fast",
    "susanin-stage-detect",
    "susanin-stage-judge"
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

/*
 * RouterOS 7.23.x can be awkward around immediate add/remove/read-back by
 * returned object id.  Staging uses stable unique names, so do not trust the
 * add ret id here.  Read the small /system script table and select the exact
 * name in the client callback.  This also makes rollback deterministic.
 */
static int lookup_scan_cb(const ros_sentence_t *s, void *opaque) {
    lookup_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;

    const char *name = ros_get_attr(s, "name");
    if (!name || strcmp(name, ctx->wanted_name) != 0) return 0;

    const char *id = ros_get_attr(s, ".id");
    const char *source = ros_get_attr(s, "source");
    const char *invalid = ros_get_attr(s, "invalid");
    if (!id || !*id) return 0;

    stage_obj_t *o = ctx->out;
    o->found = 1;
    snprintf(o->id, sizeof(o->id), "%s", id);
    snprintf(o->name, sizeof(o->name), "%s", name);
    o->invalid = attr_true(invalid);
    o->bytes = source ? strlen(source) : 0;
    susanin_fingerprint_hex(source, o->fp);
    return 0;
}

static int lookup_name_exact(ros_client_t *ros, const char *name, stage_obj_t *out) {
    memset(out, 0, sizeof(*out));
    lookup_ctx_t ctx = {.wanted_name = name, .out = out};
    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=.id,name,source,invalid,comment"
    };
    return ros_command(ros, cmd, 2, lookup_scan_cb, &ctx);
}

static int lookup_with_retry(ros_client_t *ros, const char *name, stage_obj_t *out) {
    for (int attempt = 0; attempt < 15; ++attempt) {
        int rc = lookup_name_exact(ros, name, out);
        if (rc == 0 && out->found) return 0;
        sleep_ms(100);
    }
    return -1;
}

static int remove_id(ros_client_t *ros, const char *id) {
    size_t wlen = strlen(id) + 7;
    char *wid = malloc(wlen);
    if (!wid) return -1;
    snprintf(wid, wlen, "=.id=%s", id);
    const char *cmd[] = {"/system/script/remove", wid};
    int rc = ros_command(ros, cmd, 2, NULL, NULL);
    free(wid);
    return rc;
}

static int remove_name_exact(ros_client_t *ros, const char *name) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        stage_obj_t o;
        if (lookup_name_exact(ros, name, &o) < 0) {
            sleep_ms(100);
            continue;
        }
        if (!o.found) return 0;
        if (remove_id(ros, o.id) == 0) {
            sleep_ms(100);
            stage_obj_t verify;
            if (lookup_name_exact(ros, name, &verify) == 0 && !verify.found) return 0;
        }
        sleep_ms(100);
    }
    return -1;
}

static int add_stage(ros_client_t *ros, const char *name, const char *source,
                     const char *comment) {
    size_t nlen = strlen(name) + 7;
    size_t slen = strlen(source) + 9;
    size_t clen = strlen(comment) + 10;
    char *wname = malloc(nlen);
    char *wsource = malloc(slen);
    char *wcomment = malloc(clen);
    if (!wname || !wsource || !wcomment) {
        free(wname); free(wsource); free(wcomment);
        return -1;
    }
    snprintf(wname, nlen, "=name=%s", name);
    snprintf(wsource, slen, "=source=%s", source);
    snprintf(wcomment, clen, "=comment=%s", comment);
    const char *cmd[] = {"/system/script/add", wname, wsource, wcomment};
    int rc = ros_command(ros, cmd, 4, NULL, NULL);
    free(wname); free(wsource); free(wcomment);
    return rc;
}

int stage_clean_run(ros_client_t *ros) {
    unsigned removed = 0;
    unsigned failed = 0;
    printf("=== SUSANIN STAGE CLEAN v%s ===\n", SUSANIN_VERSION);
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        stage_obj_t o;
        if (lookup_name_exact(ros, stage_names[i], &o) < 0) {
            printf("  FAIL   %s lookup failed\n", stage_names[i]);
            failed++;
            continue;
        }
        if (!o.found) {
            printf("  KEEP   %-22s absent\n", stage_names[i]);
            continue;
        }
        if (remove_name_exact(ros, stage_names[i]) < 0) {
            printf("  FAIL   %-22s remove failed id=%s\n", stage_names[i], o.id);
            failed++;
        } else {
            printf("  REMOVE %-22s id=%s\n", stage_names[i], o.id);
            removed++;
        }
    }
    printf("\nStage cleanup: removed=%u failed=%u\n", removed, failed);
    return failed == 0 ? 0 : -1;
}

int stage_run(ros_client_t *ros, const app_config_t *cfg) {
    susanin_render_bundle_t desired;
    if (renderer_build(ros, cfg, &desired) < 0) return -1;

    printf("=== SUSANIN STAGE v%s ===\n", SUSANIN_VERSION);
    printf("Mode: persistent inert stage scripts; production data-plane is untouched\n");
    printf("LAN IPv4 networks: %u\n", desired.lan_networks);
    printf("Egress: %s address=%s\n", cfg->egress_interface, desired.egress_address);
    printf("Routing table: %s\n\n", cfg->routing_table);

    /* Idempotent pre-clean by exact stable stage name. */
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        if (remove_name_exact(ros, stage_names[i]) < 0) {
            printf("  FAIL   %-18s cannot remove previous stage object %s\n",
                   desired.scripts[i].name, stage_names[i]);
            renderer_free(&desired);
            return -1;
        }
    }

    unsigned passed = 0;
    unsigned failed = 0;

    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        const susanin_rendered_script_t *s = &desired.scripts[i];
        char comment[192];
        snprintf(comment, sizeof(comment),
                 "SUSANIN:v" SUSANIN_VERSION " staged source for %s fp=%s; DO NOT RUN",
                 s->name, s->fp);

        if (add_stage(ros, stage_names[i], s->source, comment) < 0) {
            printf("  FAIL   %-18s RouterOS rejected stage object\n", s->name);
            failed++;
            break;
        }

        stage_obj_t o;
        if (lookup_with_retry(ros, stage_names[i], &o) < 0 || !o.found) {
            printf("  FAIL   %-18s stage object could not be read back\n", s->name);
            failed++;
            break;
        }

        int roundtrip = (o.bytes == s->bytes && strcmp(o.fp, s->fp) == 0);
        if (o.invalid || !roundtrip) {
            printf("  FAIL   %-18s object=%s id=%s invalid=%s roundtrip=%s desired=%zu/%s router=%zu/%s\n",
                   s->name, o.name, o.id,
                   o.invalid ? "true" : "false",
                   roundtrip ? "ok" : "mismatch",
                   s->bytes, s->fp, o.bytes, o.fp);
            failed++;
            break;
        }

        printf("  STAGE  %-18s -> %-22s id=%-6s bytes=%-6zu fnv1a64=%s invalid=false\n",
               s->name, stage_names[i], o.id, o.bytes, o.fp);
        passed++;
    }

    if (failed) {
        printf("\nStage failed; rolling back all Susanin stage objects by exact name...\n");
        unsigned cleanup_fail = 0;
        for (size_t i = 0; i < STAGE_COUNT; ++i) {
            if (remove_name_exact(ros, stage_names[i]) < 0) cleanup_fail++;
        }
        printf("Production scripts changed: NO\n");
        printf("Stage objects retained: %s\n", cleanup_fail ? "UNKNOWN (manual check required)" : "NO");
        renderer_free(&desired);
        return -1;
    }

    printf("\nStage summary: STAGED=%u FAIL=0\n", passed);
    printf("Production scripts changed: NO\n");
    printf("Schedulers attached to stage: NO\n");
    printf("Stage objects retained: YES\n");
    printf("Next safety gate: inspect stage, then promote in a later release.\n");

    renderer_free(&desired);
    return passed == STAGE_COUNT ? 0 : -1;
}
