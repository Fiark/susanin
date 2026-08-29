#include "snapshot.h"
#include "fingerprint.h"

#include <stdio.h>
#include <string.h>

static const char *managed_scripts[] = {
    "auto-awg-health",
    "auto-awg-fast",
    "auto-awg-detect",
    "auto-awg-judge"
};

typedef struct {
    unsigned found;
} snapshot_ctx_t;

static int is_managed(const char *name) {
    if (!name) return 0;
    for (size_t i = 0; i < sizeof(managed_scripts) / sizeof(managed_scripts[0]); ++i) {
        if (strcmp(name, managed_scripts[i]) == 0) return 1;
    }
    return 0;
}

static int script_cb(const ros_sentence_t *s, void *opaque) {
    snapshot_ctx_t *ctx = opaque;
    if (!ros_is_reply(s, "!re")) return 0;

    const char *name = ros_get_attr(s, "name");
    if (!is_managed(name)) return 0;

    const char *source = ros_get_attr(s, "source");
    const char *disabled = ros_get_attr(s, "disabled");
    size_t len = source ? strlen(source) : 0;
    char fp[17];
    susanin_fingerprint_hex(source, fp);

    printf("  %-18s bytes=%-6zu fnv1a64=%s disabled=%s\n",
           name, len, fp, disabled ? disabled : "false");
    ctx->found++;
    return 0;
}

int snapshot_run(ros_client_t *ros, const app_config_t *cfg) {
    (void)cfg;
    snapshot_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf("=== SUSANIN GOLDEN SNAPSHOT ===\n");
    printf("Fingerprint algorithm: FNV-1a 64-bit + source byte length\n");
    printf("Purpose: exact content drift detection, not cryptographic authentication.\n\n");
    printf("Managed RouterOS scripts:\n");

    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=name,source,disabled"
    };
    if (ros_command(ros, cmd, 2, script_cb, &ctx) < 0) return -1;

    printf("\nSnapshot summary: scripts=%u/4\n", ctx.found);
    if (ctx.found == 4) {
        printf("Golden baseline: complete\n");
        return 0;
    }

    printf("Golden baseline: incomplete\n");
    return -1;
}
