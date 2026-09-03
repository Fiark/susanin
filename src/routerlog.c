#include "routerlog.h"
#include "diag.h"

#include <stdio.h>
#include <string.h>

#define ROUTERLOG_KEEP 8
#define ROUTERLOG_TIME_MAX 64
#define ROUTERLOG_TOPICS_MAX 128
#define ROUTERLOG_MESSAGE_MAX 512

typedef struct {
    char time[ROUTERLOG_TIME_MAX];
    char topics[ROUTERLOG_TOPICS_MAX];
    char message[ROUTERLOG_MESSAGE_MAX];
} routerlog_entry_t;

typedef struct {
    const app_config_t *cfg;

    unsigned total;
    unsigned script_errors;
    unsigned no_such_item;

    routerlog_entry_t recent[ROUTERLOG_KEEP];
    unsigned recent_count;
    unsigned recent_next;

    char latest_no_such_time[ROUTERLOG_TIME_MAX];
    char latest_no_such_message[ROUTERLOG_MESSAGE_MAX];
} routerlog_ctx_t;

static int contains_topic(
    const char *topics,
    const char *needle
) {
    return topics &&
        needle &&
        strstr(topics, needle) != NULL;
}

static void copy_text(
    char *dst,
    size_t dst_size,
    const char *src
) {
    if (!dst || dst_size == 0) return;

    if (!src) src = "";

    snprintf(
        dst,
        dst_size,
        "%s",
        src
    );
}

static void remember_error(
    routerlog_ctx_t *ctx,
    const char *time,
    const char *topics,
    const char *message
) {
    routerlog_entry_t *entry =
        &ctx->recent[ctx->recent_next];

    copy_text(
        entry->time,
        sizeof(entry->time),
        time
    );

    copy_text(
        entry->topics,
        sizeof(entry->topics),
        topics
    );

    copy_text(
        entry->message,
        sizeof(entry->message),
        message
    );

    ctx->recent_next =
        (ctx->recent_next + 1U) %
        ROUTERLOG_KEEP;

    if (ctx->recent_count < ROUTERLOG_KEEP) {
        ++ctx->recent_count;
    }
}

static int log_cb(
    const ros_sentence_t *s,
    void *opaque
) {
    routerlog_ctx_t *ctx = opaque;

    if (!ros_is_reply(s, "!re")) {
        return 0;
    }

    ++ctx->total;

    const char *time =
        ros_get_attr(s, "time");

    const char *topics =
        ros_get_attr(s, "topics");

    const char *message =
        ros_get_attr(s, "message");

    if (!time) time = "-";
    if (!topics) topics = "-";
    if (!message) message = "";

    int script_error =
        (
            contains_topic(topics, "script") &&
            contains_topic(topics, "error")
        ) ||
        strstr(message, "script error") != NULL;

    if (!script_error) {
        return 0;
    }

    ++ctx->script_errors;

    remember_error(
        ctx,
        time,
        topics,
        message
    );

    if (
        strstr(
            message,
            "no such item"
        ) != NULL
    ) {
        ++ctx->no_such_item;

        copy_text(
            ctx->latest_no_such_time,
            sizeof(ctx->latest_no_such_time),
            time
        );

        copy_text(
            ctx->latest_no_such_message,
            sizeof(ctx->latest_no_such_message),
            message
        );
    }

    return 0;
}

static void print_recent_errors(
    const routerlog_ctx_t *ctx
) {
    if (ctx->recent_count == 0) {
        printf(
            "\nRecent script errors: none\n"
        );
        return;
    }

    printf(
        "\nRecent script errors (%u max):\n",
        ROUTERLOG_KEEP
    );

    unsigned start;

    if (ctx->recent_count < ROUTERLOG_KEEP) {
        start = 0;
    } else {
        start = ctx->recent_next;
    }

    for (
        unsigned i = 0;
        i < ctx->recent_count;
        ++i
    ) {
        unsigned index =
            (start + i) %
            ROUTERLOG_KEEP;

        const routerlog_entry_t *entry =
            &ctx->recent[index];

        printf(
            "  %s topics=%s message=%s\n",
            entry->time,
            entry->topics,
            entry->message
        );
    }
}

int routerlog_collect_errors(
    ros_client_t *ros,
    const app_config_t *cfg
) {
    if (!ros || !cfg) return -1;

    routerlog_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;

    /*
     * Do not rely on RouterOS-side
     * topics~"script,error" filtering.
     *
     * RouterOS 7.23.3 was observed to return
     * no rows for that CLI filter even when
     * matching records had topics=script,error.
     *
     * Read the bounded RouterOS log and filter
     * locally.
     */
    const char *cmd[] = {
        "/log/print",
        "=.proplist=time,topics,message"
    };

    printf(
        "=== SUSANIN ROUTEROS SCRIPT ERRORS ===\n"
    );

    if (
        ros_command(
            ros,
            cmd,
            2,
            log_cb,
            &ctx
        ) < 0
    ) {
        (void)diag_event(
            cfg,
            "telemetry_error",
            "RouterOS log query failed"
        );

        return -1;
    }

    printf(
        "Log records scanned : %u\n",
        ctx.total
    );

    printf(
        "Script errors       : %u\n",
        ctx.script_errors
    );

    printf(
        "no such item        : %u\n",
        ctx.no_such_item
    );

    if (ctx.no_such_item > 0) {
        printf(
            "Latest no such item: %s %s\n",
            ctx.latest_no_such_time,
            ctx.latest_no_such_message
        );
    }

    print_recent_errors(&ctx);

    char summary[1024];

    if (ctx.no_such_item > 0) {
        snprintf(
            summary,
            sizeof(summary),
            "records=%u script_errors=%u no_such_item=%u "
            "latest_no_such_time=%s latest_no_such_message=%s",
            ctx.total,
            ctx.script_errors,
            ctx.no_such_item,
            ctx.latest_no_such_time,
            ctx.latest_no_such_message
        );
    } else {
        snprintf(
            summary,
            sizeof(summary),
            "records=%u script_errors=%u no_such_item=0",
            ctx.total,
            ctx.script_errors
        );
    }

    (void)diag_event(
        cfg,
        "routeros_error_summary",
        summary
    );

    return 0;
}
