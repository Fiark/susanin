#define _POSIX_C_SOURCE 200809L
#include "validate.h"
#include "fingerprint.h"
#include "renderer.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char id[64];
    int found;
    int invalid;
    size_t bytes;
    char fp[17];
} temp_script_t;

typedef struct {
    char id[64];
} add_result_t;

static int attr_true(const char *v) {
    return v && *v && (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0);
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0) { }
}

static int temp_cb(const ros_sentence_t *s, void *opaque) {
    temp_script_t *t = opaque;
    if (!ros_is_reply(s, "!re")) return 0;
    const char *id = ros_get_attr(s, ".id");
    const char *source = ros_get_attr(s, "source");
    const char *invalid = ros_get_attr(s, "invalid");
    if (!id || !*id) return 0;
    t->found = 1;
    snprintf(t->id, sizeof(t->id), "%s", id);
    t->invalid = attr_true(invalid);
    t->bytes = source ? strlen(source) : 0;
    susanin_fingerprint_hex(source, t->fp);
    return 0;
}

static int add_cb(const ros_sentence_t *s, void *opaque) {
    add_result_t *r = opaque;
    if (!ros_is_reply(s, "!done")) return 0;
    const char *ret = ros_get_attr(s, "ret");
    if (ret && *ret) snprintf(r->id, sizeof(r->id), "%s", ret);
    return 0;
}

static int lookup_temp_name(ros_client_t *ros, const char *name, temp_script_t *out) {
    char q[160];
    snprintf(q, sizeof(q), "?name=%s", name);
    memset(out, 0, sizeof(*out));
    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=.id,name,source,invalid",
        q
    };
    return ros_command(ros, cmd, 3, temp_cb, out);
}

static int lookup_temp_id(ros_client_t *ros, const char *id, temp_script_t *out) {
    char q[128];
    snprintf(q, sizeof(q), "?.id=%s", id);
    memset(out, 0, sizeof(*out));
    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=.id,name,source,invalid",
        q
    };
    return ros_command(ros, cmd, 3, temp_cb, out);
}

static int lookup_with_retry(ros_client_t *ros, const char *id, const char *name,
                             temp_script_t *out) {
    for (int attempt = 0; attempt < 12; ++attempt) {
        int rc = (id && *id) ? lookup_temp_id(ros, id, out)
                             : lookup_temp_name(ros, name, out);
        if (rc == 0 && out->found) return 0;
        sleep_ms(100);
    }
    return -1;
}

static int remove_id(ros_client_t *ros, const char *id) {
    char wid[96];
    snprintf(wid, sizeof(wid), "=.id=%s", id);
    const char *cmd[] = {"/system/script/remove", wid};
    return ros_command(ros, cmd, 2, NULL, NULL);
}

static int add_temp(ros_client_t *ros, const char *name, const char *source,
                    add_result_t *result) {
    size_t nlen = strlen(name) + 7;
    size_t slen = strlen(source) + 9;
    char *wname = malloc(nlen);
    char *wsource = malloc(slen);
    if (!wname || !wsource) {
        free(wname);
        free(wsource);
        return -1;
    }
    snprintf(wname, nlen, "=name=%s", name);
    snprintf(wsource, slen, "=source=%s", source);
    const char *cmd[] = {
        "/system/script/add",
        wname,
        wsource,
        "=comment=SUSANIN: temporary syntax validation object"
    };
    memset(result, 0, sizeof(*result));
    int rc = ros_command(ros, cmd, 4, add_cb, result);
    free(wname);
    free(wsource);
    return rc;
}

static int best_effort_remove(ros_client_t *ros, const char *id, const char *name) {
    if (id && *id && remove_id(ros, id) == 0) return 0;

    /* If RouterOS already removed/renumbered the object, verify by name. */
    sleep_ms(100);
    temp_script_t t;
    if (lookup_temp_name(ros, name, &t) == 0 && !t.found) return 0;
    if (t.found && remove_id(ros, t.id) == 0) return 0;
    return -1;
}

int validate_run(ros_client_t *ros, const app_config_t *cfg) {
    susanin_render_bundle_t desired;
    if (renderer_build(ros, cfg, &desired) < 0) return -1;

    printf("=== SUSANIN VALIDATE v%s ===\n", SUSANIN_VERSION);
    printf("Mode: unique temporary RouterOS script objects; production data-plane is untouched\n");
    printf("LAN IPv4 networks: %u\n", desired.lan_networks);
    printf("Egress: %s address=%s\n", cfg->egress_interface, desired.egress_address);
    printf("Routing table: %s\n\n", cfg->routing_table);

    unsigned passed = 0;
    unsigned failed = 0;
    unsigned long stamp = (unsigned long)time(NULL);
    long pid = (long)getpid();

    for (size_t i = 0; i < SUSANIN_SCRIPT_COUNT; ++i) {
        const susanin_rendered_script_t *s = &desired.scripts[i];
        const char *short_name = s->name + strlen("auto-awg-");
        char temp_name[128];
        snprintf(temp_name, sizeof(temp_name), "susanin-validate-%lu-%ld-%zu-%s",
                 stamp, pid, i, short_name);

        add_result_t added;
        if (add_temp(ros, temp_name, s->source, &added) < 0) {
            printf("  FAIL   %-18s RouterOS rejected temporary script add\n", s->name);
            failed++;
            continue;
        }

        temp_script_t t;
        if (lookup_with_retry(ros, added.id, temp_name, &t) < 0 || !t.found) {
            printf("  FAIL   %-18s temporary object could not be read back after retries\n", s->name);
            failed++;
            if (best_effort_remove(ros, added.id, temp_name) < 0)
                printf("         warning: temporary object cleanup needs manual check: %s\n", temp_name);
            continue;
        }

        int roundtrip = (t.bytes == s->bytes && strcmp(t.fp, s->fp) == 0);
        if (t.invalid || !roundtrip) {
            printf("  FAIL   %-18s invalid=%s roundtrip=%s desired=%zu/%s router=%zu/%s\n",
                   s->name,
                   t.invalid ? "true" : "false",
                   roundtrip ? "ok" : "mismatch",
                   s->bytes, s->fp, t.bytes, t.fp);
            failed++;
        } else {
            printf("  PASS   %-18s bytes=%-6zu fnv1a64=%s invalid=false\n",
                   s->name, t.bytes, t.fp);
            passed++;
        }

        if (best_effort_remove(ros, t.id, temp_name) < 0) {
            printf("         warning: failed to remove temporary object %s\n", temp_name);
            failed++;
        }
    }

    printf("\nValidation summary: PASS=%u FAIL=%u\n", passed, failed);
    printf("Production scripts changed: NO\n");
    printf("Temporary validator objects remaining should be: 0\n");

    renderer_free(&desired);
    return failed == 0 && passed == SUSANIN_SCRIPT_COUNT ? 0 : -1;
}
