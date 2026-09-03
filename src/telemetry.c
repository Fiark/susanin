#include "telemetry.h"
#include "diag.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char uptime[64];
    char version[64];
    char cpu_load[32];
    char free_memory[64];
    char total_memory[64];
    char board_name[128];
    char architecture[64];
    int seen;
} resource_ctx_t;

typedef struct {
    char total_entries[64];
    char max_entries[64];
    int seen;
} conntrack_ctx_t;

typedef struct {
    unsigned total;
    unsigned managed_workers;
    unsigned agent_jobs;

    char managed_names[512];
    size_t managed_names_used;

    char all_jobs[2048];
    size_t all_jobs_used;
} jobs_ctx_t;

static void copy_attr(
    char *dst,
    size_t dst_size,
    const ros_sentence_t *s,
    const char *name
) {
    const char *value = ros_get_attr(s, name);

    if (!dst || dst_size == 0) return;

    if (!value) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", value);
}

static int resource_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    resource_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) return 0;

    copy_attr(
        ctx->uptime,
        sizeof(ctx->uptime),
        s,
        "uptime"
    );

    copy_attr(
        ctx->version,
        sizeof(ctx->version),
        s,
        "version"
    );

    copy_attr(
        ctx->cpu_load,
        sizeof(ctx->cpu_load),
        s,
        "cpu-load"
    );

    copy_attr(
        ctx->free_memory,
        sizeof(ctx->free_memory),
        s,
        "free-memory"
    );

    copy_attr(
        ctx->total_memory,
        sizeof(ctx->total_memory),
        s,
        "total-memory"
    );

    copy_attr(
        ctx->board_name,
        sizeof(ctx->board_name),
        s,
        "board-name"
    );

    copy_attr(
        ctx->architecture,
        sizeof(ctx->architecture),
        s,
        "architecture-name"
    );

    ctx->seen = 1;
    return 0;
}

static int conntrack_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    conntrack_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) return 0;

    copy_attr(
        ctx->total_entries,
        sizeof(ctx->total_entries),
        s,
        "total-entries"
    );

    copy_attr(
        ctx->max_entries,
        sizeof(ctx->max_entries),
        s,
        "max-entries"
    );

    ctx->seen = 1;
    return 0;
}

static void append_job_name(
    char *buf,
    size_t buf_size,
    size_t *used,
    const char *name
) {
    if (
        !buf ||
        !used ||
        !name ||
        !*name ||
        *used >= buf_size - 1
    ) {
        return;
    }

    int written = snprintf(
        buf + *used,
        buf_size - *used,
        "%s%s",
        *used ? "," : "",
        name
    );

    if (written < 0) return;

    size_t added = (size_t)written;

    if (added >= buf_size - *used) {
        *used = buf_size - 1;
    } else {
        *used += added;
    }
}

static int jobs_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    jobs_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) return 0;

    ++ctx->total;

    const char *script = ros_get_attr(s, "script");
    const char *owner = ros_get_attr(s, "owner");
    const char *type = ros_get_attr(s, "type");
    const char *started = ros_get_attr(s, "started");

    if (!script || !*script) script = "-";
    if (!owner || !*owner) owner = "-";
    if (!type || !*type) type = "-";
    if (!started || !*started) started = "-";

    char one_job[512];

    snprintf(
        one_job,
        sizeof(one_job),
        "{script=%s owner=%s type=%s started=%s}",
        script,
        owner,
        type,
        started
    );

    append_job_name(
        ctx->all_jobs,
        sizeof(ctx->all_jobs),
        &ctx->all_jobs_used,
        one_job
    );

    if (strcmp(owner, "susanin-agent") == 0) {
        ++ctx->agent_jobs;
    }

    if (
        strcmp(script, "-") != 0 &&
        strncmp(script, "auto-awg-", 9) == 0
    ) {
        ++ctx->managed_workers;

        append_job_name(
            ctx->managed_names,
            sizeof(ctx->managed_names),
            &ctx->managed_names_used,
            script
        );
    }

    return 0;
}

static int collect_resource(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    resource_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    const char *cmd[] = {
        "/system/resource/print",
        "=.proplist=uptime,version,cpu-load,"
        "free-memory,total-memory,"
        "board-name,architecture-name"
    };

    if (
        ros_command(
            ros,
            cmd,
            2,
            resource_cb,
            &ctx
        ) < 0
    ) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "system resource query failed"
        );

        return -1;
    }

    if (!ctx.seen) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "system resource query returned no data"
        );

        return -1;
    }

    char msg[768];

    snprintf(
        msg,
        sizeof(msg),
        "cpu_load=%s "
        "free_memory=%s "
        "total_memory=%s "
        "uptime=%s "
        "version=%s "
        "board=%s "
        "arch=%s",
        ctx.cpu_load[0] ? ctx.cpu_load : "?",
        ctx.free_memory[0] ? ctx.free_memory : "?",
        ctx.total_memory[0] ? ctx.total_memory : "?",
        ctx.uptime[0] ? ctx.uptime : "?",
        ctx.version[0] ? ctx.version : "?",
        ctx.board_name[0] ? ctx.board_name : "?",
        ctx.architecture[0] ? ctx.architecture : "?"
    );

    printf(
        "Resource: CPU=%s%% free-memory=%s "
        "total-memory=%s uptime=%s\n",
        ctx.cpu_load[0] ? ctx.cpu_load : "?",
        ctx.free_memory[0] ? ctx.free_memory : "?",
        ctx.total_memory[0] ? ctx.total_memory : "?",
        ctx.uptime[0] ? ctx.uptime : "?"
    );

    return diag_event(
        cfg,
        "router_metrics",
        msg
    );
}

static int collect_conntrack(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    conntrack_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    const char *cmd[] = {
        "/ip/firewall/connection/tracking/print",
        "=.proplist=total-entries,max-entries"
    };

    if (
        ros_command(
            ros,
            cmd,
            2,
            conntrack_cb,
            &ctx
        ) < 0
    ) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "connection tracking metrics query failed"
        );

        return -1;
    }

    if (!ctx.seen) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "connection tracking metrics returned no data"
        );

        return -1;
    }

    char msg[256];

    snprintf(
        msg,
        sizeof(msg),
        "total_entries=%s max_entries=%s",
        ctx.total_entries[0]
            ? ctx.total_entries
            : "?",
        ctx.max_entries[0]
            ? ctx.max_entries
            : "?"
    );

    printf(
        "Conntrack: total=%s max=%s\n",
        ctx.total_entries[0]
            ? ctx.total_entries
            : "?",
        ctx.max_entries[0]
            ? ctx.max_entries
            : "?"
    );

    return diag_event(
        cfg,
        "conntrack_metrics",
        msg
    );
}

static int collect_jobs(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    jobs_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    const char *cmd[] = {
        "/system/script/job/print",
        "=.proplist=script,owner,type,started"
    };

    if (
        ros_command(
            ros,
            cmd,
            2,
            jobs_cb,
            &ctx
        ) < 0
    ) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "script job query failed"
        );

        return -1;
    }

    char msg[4096];

    snprintf(
        msg,
        sizeof(msg),
        "total=%u managed_workers=%u agent_jobs=%u "
        "managed=%s jobs=%s",
        ctx.total,
        ctx.managed_workers,
        ctx.agent_jobs,
        ctx.managed_names[0]
            ? ctx.managed_names
            : "-",
        ctx.all_jobs[0]
            ? ctx.all_jobs
            : "-"
    );

    printf(
        "Script jobs: total=%u managed-workers=%u agent-jobs=%u\n",
        ctx.total,
        ctx.managed_workers,
        ctx.agent_jobs
    );

    printf(
        "  Workers: %s\n",
        ctx.managed_names[0]
            ? ctx.managed_names
            : "-"
    );

    printf(
        "  Jobs   : %s\n",
        ctx.all_jobs[0]
            ? ctx.all_jobs
            : "-"
    );

    return diag_event(
        cfg,
        "script_jobs",
        msg
    );
}

int telemetry_collect_once(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    if (!ros || !cfg) return -1;

    printf(
        "=== SUSANIN ROUTEROS TELEMETRY ===\n"
    );

    int failed = 0;

    if (collect_resource(ros, cfg) < 0) {
        failed = 1;
    }

    if (collect_conntrack(ros, cfg) < 0) {
        failed = 1;
    }

    if (collect_jobs(ros, cfg) < 0) {
        failed = 1;
    }

    if (failed) {
        printf(
            "Telemetry result: PARTIAL/FAILED\n"
        );
        return -1;
    }

    printf("Telemetry result: OK\n");
    return 0;
}
