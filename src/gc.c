#define _POSIX_C_SOURCE 200809L

#include "gc.h"

#include "fingerprint.h"
#include "renderer.h"
#include "state_cleanup.h"
#include "version.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#define GC_LAZY_TEST_BUDGET   4U
#define GC_LAZY_OK_BUDGET     4U
#define GC_MARK_TEST_BUDGET   2U
#define GC_MARK_OK_BUDGET     2U

#define GC_MAX_STATIC_STATE   32U
#define GC_MAX_LAZY           128U
#define GC_MAX_MARKS          128U
#define GC_MARK_SCAN_RETRIES  3U
#define GC_MARK_VERIFY_RETRIES 20U

_Static_assert(
    GC_LAZY_TEST_BUDGET +
    GC_LAZY_OK_BUDGET +
    GC_MARK_TEST_BUDGET +
    GC_MARK_OK_BUDGET
        <= SUSANIN_GC_TOTAL_BUDGET,
    "GC family budgets exceed global mutation budget"
);

#define SUSANIN_RUNTIME_LOCK_PATH "/data/susanin-runtime.lock"

int susanin_runtime_lock_try(
    int *fd_out
)
{
    if (!fd_out) return -1;

    *fd_out = -1;

    int fd = open(
        SUSANIN_RUNTIME_LOCK_PATH,
        O_CREAT | O_RDWR,
        0600
    );

    if (fd < 0) return -1;

    if (
        flock(
            fd,
            LOCK_EX | LOCK_NB
        ) < 0
    ) {
        int saved_errno = errno;

        close(fd);

        if (
            saved_errno == EWOULDBLOCK ||
            saved_errno == EAGAIN
        ) {
            return 1;
        }

        return -1;
    }

    *fd_out = fd;
    return 0;
}

void susanin_runtime_lock_release(
    int fd
)
{
    if (fd < 0) return;

    (void)flock(
        fd,
        LOCK_UN
    );

    close(fd);
}

static const char *prod_names[] = {
    "auto-awg-health",
    "auto-awg-fast",
    "auto-awg-detect",
    "auto-awg-judge"
};

static const char *fixed_comments[] = {
    "AUTO-AWG: L2 mark confirmed",
    "AUTO-AWG: L1 mark test",
    "AUTO-AWG: L2 mark confirmed UDP",
    "AUTO-AWG: L1 mark test UDP",
    "AUTO-AWG: L2 route confirmed",
    "AUTO-AWG: L1 route test",
    "AUTO-AWG: router DNS UDP via tunnel",
    "AUTO-AWG: router DNS TCP via tunnel"
};

typedef enum {
    GC_KIND_TEST = 0,
    GC_KIND_OK = 1
} gc_kind_t;

static void sleep_ms(long ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000L;
    ts.tv_nsec = (ms % 1000L) * 1000000L;

    while (nanosleep(&ts, &ts) < 0) {
    }
}

static int attr_true(const char *value)
{
    return
        value &&
        *value &&
        (
            strcmp(value, "true") == 0 ||
            strcmp(value, "yes") == 0
        );
}

static int starts_with(const char *value, const char *prefix)
{
    return
        value &&
        prefix &&
        strncmp(value, prefix, strlen(prefix)) == 0;
}

static const char *gc_mark_name(gc_kind_t kind)
{
    return
        kind == GC_KIND_TEST
            ? "auto-awg-test-conn"
            : "auto-awg-ok-conn";
}

static const char *gc_state_name(gc_kind_t kind)
{
    return
        kind == GC_KIND_TEST
            ? "test"
            : "ok";
}

static int parse_port(const char *value, unsigned *out)
{
    if (!value || !*value || !out) return -1;

    char *end = NULL;
    unsigned long port = strtoul(value, &end, 10);

    if (
        !end ||
        *end ||
        port == 0UL ||
        port > 65535UL
    ) {
        return -1;
    }

    *out = (unsigned)port;
    return 0;
}

static void make_state_list(
    char out[128],
    const char *family,
    const char *proto,
    unsigned port
)
{
    snprintf(
        out,
        128,
        "auto_awg_%s_%s_%u",
        family,
        proto,
        port
    );
}

/* ------------------------------------------------------------------------- */
/* Exact active-v0.12 ownership gate                                        */
/* ------------------------------------------------------------------------- */

typedef struct {
    const char *wanted;
    unsigned count;
    size_t bytes;
    char fp[17];
    int invalid;
} script_gate_ctx_t;

static int script_gate_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    script_gate_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *name = ros_get_attr(s, "name");

    if (!name || strcmp(name, ctx->wanted) != 0) return 0;

    const char *source = ros_get_attr(s, "source");

    ctx->count++;
    ctx->invalid = attr_true(ros_get_attr(s, "invalid"));
    ctx->bytes = strlen(source ? source : "");
    susanin_fingerprint_hex(source ? source : "", ctx->fp);

    return 0;
}

static int query_script_gate(
    ros_client_t *ros,
    const char *name,
    script_gate_ctx_t *out
)
{
    memset(out, 0, sizeof(*out));
    out->wanted = name;

    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?name=%s",
        name
    );

    const char *cmd[] = {
        "/system/script/print",
        "=.proplist=name,source,invalid",
        query
    };

    return ros_command(
        ros,
        cmd,
        3,
        script_gate_cb,
        out
    );
}

typedef struct {
    const char *wanted;
    unsigned count;
    char id[64];
    char on_event[128];
    int disabled;
} scheduler_gate_ctx_t;

static int scheduler_gate_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    scheduler_gate_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *name = ros_get_attr(s, "name");

    if (!name || strcmp(name, ctx->wanted) != 0) return 0;

    ctx->count++;

    const char *id = ros_get_attr(s, ".id");
    const char *on_event = ros_get_attr(s, "on-event");

    snprintf(
        ctx->id,
        sizeof(ctx->id),
        "%s",
        id ? id : ""
    );

    snprintf(
        ctx->on_event,
        sizeof(ctx->on_event),
        "%s",
        on_event ? on_event : ""
    );

    ctx->disabled = attr_true(
        ros_get_attr(
            s,
            "disabled"
        )
    );

    return 0;
}

static int query_scheduler_gate(
    ros_client_t *ros,
    const char *name,
    scheduler_gate_ctx_t *out
)
{
    memset(out, 0, sizeof(*out));
    out->wanted = name;

    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?name=%s",
        name
    );

    const char *cmd[] = {
        "/system/scheduler/print",
        "=.proplist=.id,name,on-event,disabled",
        query
    };

    return ros_command(
        ros,
        cmd,
        3,
        scheduler_gate_cb,
        out
    );
}

typedef struct {
    const char *wanted;
    unsigned count;
    int disabled;
} fixed_gate_ctx_t;

static int fixed_gate_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    fixed_gate_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *comment = ros_get_attr(s, "comment");

    if (
        !comment ||
        strcmp(comment, ctx->wanted) != 0
    ) {
        return 0;
    }

    ctx->count++;
    ctx->disabled = attr_true(
        ros_get_attr(
            s,
            "disabled"
        )
    );

    return 0;
}

static int query_fixed_gate(
    ros_client_t *ros,
    const char *comment,
    fixed_gate_ctx_t *out
)
{
    memset(out, 0, sizeof(*out));
    out->wanted = comment;

    char query[320];

    snprintf(
        query,
        sizeof(query),
        "?comment=%s",
        comment
    );

    const char *cmd[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=comment,disabled",
        query
    };

    return ros_command(
        ros,
        cmd,
        3,
        fixed_gate_cb,
        out
    );
}

typedef struct {
    unsigned count;
} count_ctx_t;

static int count_re_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    count_ctx_t *ctx = opaque;

    if (
        ctx &&
        ros_is_reply(
            s,
            "!re"
        )
    ) {
        ctx->count++;
    }

    return 0;
}

static int managed_job_count(
    ros_client_t *ros,
    const char *script,
    unsigned *out
)
{
    count_ctx_t ctx = {0};

    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?script=%s",
        script
    );

    const char *cmd[] = {
        "/system/script/job/print",
        "=.proplist=script",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            count_re_cb,
            &ctx
        ) < 0
    ) {
        return -1;
    }

    *out = ctx.count;
    return 0;
}

/*
 * Return:
 *
 *   0  normal active v0.12 production proven
 *   1  safe skip; ownership/normal-runtime gate not satisfied
 *  -1  v0.12 ownership was proven, then a later safety query failed
 */
static int gc_normal_runtime_gate(
    ros_client_t *ros,
    const app_config_t *cfg,
    int verbose
)
{
    susanin_render_bundle_t desired;

    if (
        renderer_build(
            ros,
            cfg,
            &desired
        ) < 0
    ) {
        if (verbose) {
            printf(
                "GC gate: SKIP desired source cannot be rendered safely.\n"
            );
        }

        return 1;
    }

    int result = 0;
    int ownership_proven = 0;

    for (size_t i = 0; i < SUSANIN_SCRIPT_COUNT; ++i) {
        script_gate_ctx_t current;

        if (
            query_script_gate(
                ros,
                prod_names[i],
                &current
            ) < 0
        ) {
            if (verbose) {
                printf(
                    "GC gate: SKIP production ownership query failed for %s.\n",
                    prod_names[i]
                );
            }

            result = 1;
            goto out;
        }

        if (
            current.count != 1U ||
            current.invalid ||
            current.bytes != desired.scripts[i].bytes ||
            strcmp(
                current.fp,
                desired.scripts[i].fp
            ) != 0
        ) {
            if (verbose) {
                printf(
                    "GC gate: SKIP production %s is not exact current v0.12 desired source.\n",
                    prod_names[i]
                );
            }

            result = 1;
            goto out;
        }
    }

    ownership_proven = 1;

    for (
        size_t i = 0;
        i < sizeof(prod_names) / sizeof(prod_names[0]);
        ++i
    ) {
        scheduler_gate_ctx_t sched;

        if (
            query_scheduler_gate(
                ros,
                prod_names[i],
                &sched
            ) < 0
        ) {
            result = -1;
            goto out;
        }

        if (
            sched.count != 1U ||
            sched.disabled ||
            strcmp(
                sched.on_event,
                prod_names[i]
            ) != 0
        ) {
            if (verbose) {
                printf(
                    "GC gate: SKIP scheduler %s is not in normal enabled state.\n",
                    prod_names[i]
                );
            }

            result = 1;
            goto out;
        }

        unsigned jobs = 0;

        if (
            managed_job_count(
                ros,
                prod_names[i],
                &jobs
            ) < 0
        ) {
            result = -1;
            goto out;
        }

        if (jobs != 0U) {
            if (verbose) {
                printf(
                    "GC gate: SKIP managed job active for %s.\n",
                    prod_names[i]
                );
            }

            result = 1;
            goto out;
        }
    }

    for (
        size_t i = 0;
        i < sizeof(fixed_comments) / sizeof(fixed_comments[0]);
        ++i
    ) {
        fixed_gate_ctx_t fixed;

        if (
            query_fixed_gate(
                ros,
                fixed_comments[i],
                &fixed
            ) < 0
        ) {
            result = -1;
            goto out;
        }

        if (
            fixed.count != 1U ||
            fixed.disabled
        ) {
            if (verbose) {
                printf(
                    "GC gate: SKIP fixed adaptive rule is not normal/enabled: %s\n",
                    fixed_comments[i]
                );
            }

            result = 1;
            goto out;
        }
    }

    if (verbose) {
        printf(
            "GC gate: PASS exact active v0.12 production, schedulers idle/enabled, fixed8 enabled.\n"
        );
    }

out:
    renderer_free(
        &desired
    );

    /*
     * Before exact production ownership is proven, never mutate RouterOS:
     * this is the stable-v0.11.x protection boundary.
     */
    if (!ownership_proven && result < 0) return 1;

    return result;
}

/* ------------------------------------------------------------------------- */
/* Failed-safe fallback                                                      */
/* ------------------------------------------------------------------------- */

static int disable_scheduler_exact(
    ros_client_t *ros,
    const char *name
)
{
    scheduler_gate_ctx_t sched;

    if (
        query_scheduler_gate(
            ros,
            name,
            &sched
        ) < 0 ||
        sched.count != 1U ||
        !sched.id[0]
    ) {
        return -1;
    }

    char id_word[96];

    snprintf(
        id_word,
        sizeof(id_word),
        "=.id=%s",
        sched.id
    );

    const char *cmd[] = {
        "/system/scheduler/set",
        id_word,
        "=disabled=true"
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            NULL,
            NULL
        ) < 0
    ) {
        return -1;
    }

    if (
        query_scheduler_gate(
            ros,
            name,
            &sched
        ) < 0
    ) {
        return -1;
    }

    return
        sched.count == 1U &&
        sched.disabled
            ? 0
            : -1;
}

static int gc_fail_safe(
    ros_client_t *ros,
    const char *reason
)
{
    fprintf(
        stderr,
        "Susanin GC: FAILED SAFE: %s\n",
        reason ? reason : "runtime cleanup uncertainty"
    );

    int sched_failed = 0;

    for (
        size_t i = 0;
        i < sizeof(prod_names) / sizeof(prod_names[0]);
        ++i
    ) {
        if (
            disable_scheduler_exact(
                ros,
                prod_names[i]
            ) < 0
        ) {
            sched_failed++;
        }
    }

    int mangle_rc =
        susanin_disable_adaptive_mangle(
            ros
        );

    fprintf(
        stderr,
        "Susanin GC: fail-open schedulers-paused=%s adaptive-mangle-disable=%s\n",
        sched_failed == 0 ? "SUCCESS" : "FAILED",
        mangle_rc == 0 ? "SUCCESS" : "FAILED"
    );

    fprintf(
        stderr,
        "Susanin GC: manual inspection required; independent AWG ownership was not modified.\n"
    );

    return -1;
}

/* ------------------------------------------------------------------------- */
/* Generic exact removals / verification                                     */
/* ------------------------------------------------------------------------- */

static int remove_id(
    ros_client_t *ros,
    const char *remove_cmd,
    const char *id
)
{
    char id_word[96];

    snprintf(
        id_word,
        sizeof(id_word),
        "=.id=%s",
        id
    );

    const char *cmd[] = {
        remove_cmd,
        id_word
    };

    return ros_command(
        ros,
        cmd,
        2,
        NULL,
        NULL
    );
}

typedef struct {
    const char *wanted_id;
    int found;
} id_find_ctx_t;

static int id_find_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    id_find_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *id = ros_get_attr(s, ".id");

    if (
        id &&
        strcmp(
            id,
            ctx->wanted_id
        ) == 0
    ) {
        ctx->found = 1;
    }

    return 0;
}


typedef struct {
    unsigned count;
    int seen;
} gc_count_only_ctx_t;

static int gc_count_only_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    gc_count_only_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!done")) return 0;

    const char *ret = ros_get_attr(s, "ret");

    if (!ret || !*ret) return -1;

    char *end = NULL;
    unsigned long value = strtoul(ret, &end, 10);

    if (
        !end ||
        *end ||
        value > UINT_MAX
    ) {
        return -1;
    }

    ctx->count = (unsigned)value;
    ctx->seen = 1;

    return 0;
}

static int gc_count_filtered_one(
    ros_client_t *ros,
    const char *print_cmd,
    const char *query,
    unsigned *out
)
{
    if (!ros || !print_cmd || !query || !out) return -1;

    gc_count_only_ctx_t ctx = {0};

    const char *cmd[] = {
        print_cmd,
        "=count-only=",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            gc_count_only_cb,
            &ctx
        ) < 0 ||
        !ctx.seen
    ) {
        return -1;
    }

    *out = ctx.count;
    return 0;
}

static int gc_count_filtered_two_and(
    ros_client_t *ros,
    const char *print_cmd,
    const char *query1,
    const char *query2,
    unsigned *out
)
{
    if (
        !ros ||
        !print_cmd ||
        !query1 ||
        !query2 ||
        !out
    ) {
        return -1;
    }

    gc_count_only_ctx_t ctx = {0};

    const char *cmd[] = {
        print_cmd,
        "=count-only=",
        query1,
        query2,
        "?#&"
    };

    if (
        ros_command(
            ros,
            cmd,
            5,
            gc_count_only_cb,
            &ctx
        ) < 0 ||
        !ctx.seen
    ) {
        return -1;
    }

    *out = ctx.count;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Lazy AUTO-AWG: P rules                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    char id[64];
    char comment[192];
    char protocol[16];
    char dst_port[32];
    char dst_list[128];

    gc_kind_t query_kind;
    gc_kind_t parsed_kind;

    char parsed_proto[8];
    unsigned parsed_port;

    int parsed;
    int shape_unverifiable;
    int shape_valid;
} lazy_candidate_t;

typedef struct {
    lazy_candidate_t items[GC_MAX_LAZY];
    size_t count;
    int truncated;
    gc_kind_t query_kind;
} lazy_collect_ctx_t;

static int parse_lazy_comment(
    const char *comment,
    char proto[8],
    unsigned *port,
    gc_kind_t *kind
)
{
    if (!comment || !proto || !port || !kind) return -1;

    char proto_buf[8] = {0};
    char state[8] = {0};
    char extra = '\0';
    unsigned parsed_port = 0;

    int n = sscanf(
        comment,
        "AUTO-AWG: P %7s %u %7s %c",
        proto_buf,
        &parsed_port,
        state,
        &extra
    );

    if (
        n != 3 ||
        (
            strcmp(proto_buf, "tcp") != 0 &&
            strcmp(proto_buf, "udp") != 0
        ) ||
        parsed_port == 0U ||
        parsed_port > 65535U
    ) {
        return -1;
    }

    gc_kind_t parsed_kind;

    if (strcmp(state, "TEST") == 0) {
        parsed_kind = GC_KIND_TEST;
    } else if (strcmp(state, "OK") == 0) {
        parsed_kind = GC_KIND_OK;
    } else {
        return -1;
    }

    snprintf(
        proto,
        8,
        "%s",
        proto_buf
    );

    *port = parsed_port;
    *kind = parsed_kind;

    return 0;
}

static int lazy_collect_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    lazy_collect_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *comment = ros_get_attr(s, "comment");

    if (
        !comment ||
        !starts_with(
            comment,
            "AUTO-AWG: P "
        )
    ) {
        return 0;
    }

    if (ctx->count >= GC_MAX_LAZY) {
        ctx->truncated = 1;
        return 0;
    }

    lazy_candidate_t *item =
        &ctx->items[ctx->count++];

    memset(
        item,
        0,
        sizeof(*item)
    );

    item->query_kind = ctx->query_kind;

    const char *id = ros_get_attr(s, ".id");
    const char *protocol = ros_get_attr(s, "protocol");
    const char *dst_port = ros_get_attr(s, "dst-port");
    const char *dst_list = ros_get_attr(s, "dst-address-list");

    snprintf(
        item->id,
        sizeof(item->id),
        "%s",
        id ? id : ""
    );

    snprintf(
        item->comment,
        sizeof(item->comment),
        "%s",
        comment
    );

    snprintf(
        item->protocol,
        sizeof(item->protocol),
        "%s",
        protocol ? protocol : ""
    );

    snprintf(
        item->dst_port,
        sizeof(item->dst_port),
        "%s",
        dst_port ? dst_port : ""
    );

    snprintf(
        item->dst_list,
        sizeof(item->dst_list),
        "%s",
        dst_list ? dst_list : ""
    );

    if (
        parse_lazy_comment(
            comment,
            item->parsed_proto,
            &item->parsed_port,
            &item->parsed_kind
        ) == 0
    ) {
        item->parsed = 1;
    }

    /*
     * Missing rule properties are not treated as proof of malformed state.
     * Keep the rule if RouterOS did not return enough information.
     */
    if (
        !item->id[0] ||
        !item->protocol[0] ||
        !item->dst_port[0] ||
        !item->dst_list[0]
    ) {
        item->shape_unverifiable = 1;
        return 0;
    }

    if (!item->parsed) {
        item->shape_valid = 0;
        return 0;
    }

    unsigned rule_port = 0;

    if (
        parse_port(
            item->dst_port,
            &rule_port
        ) < 0
    ) {
        item->shape_valid = 0;
        return 0;
    }

    char expected_list[128];

    make_state_list(
        expected_list,
        gc_state_name(item->parsed_kind),
        item->parsed_proto,
        item->parsed_port
    );

    item->shape_valid =
        item->parsed_kind == item->query_kind &&
        strcmp(
            item->protocol,
            item->parsed_proto
        ) == 0 &&
        rule_port == item->parsed_port &&
        strcmp(
            item->dst_list,
            expected_list
        ) == 0;

    return 0;
}

static int collect_lazy_kind(
    ros_client_t *ros,
    gc_kind_t kind,
    lazy_collect_ctx_t *out
)
{
    memset(
        out,
        0,
        sizeof(*out)
    );

    out->query_kind = kind;

    char query[128];

    snprintf(
        query,
        sizeof(query),
        "?new-connection-mark=%s",
        gc_mark_name(kind)
    );

    unsigned total = 0;

    if (
        gc_count_filtered_one(
            ros,
            "/ip/firewall/mangle/print",
            query,
            &total
        ) < 0
    ) {
        return -1;
    }

    if (total > GC_MAX_LAZY) {
        fprintf(
            stderr,
            "Susanin GC: lazy namespace exceeds bounded scan cap: %u > %u.\n",
            total,
            GC_MAX_LAZY
        );

        out->truncated = 1;
        return -1;
    }

    const char *cmd[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=.id,comment,protocol,dst-port,dst-address-list,new-connection-mark,disabled",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            lazy_collect_cb,
            out
        ) < 0
    ) {
        return -1;
    }

    /*
     * Scheduler state was idle at the ownership gate, but a new run may
     * start after it. Detect growth between count-only and detail scan.
     */
    if (out->truncated) {
        fprintf(
            stderr,
            "Susanin GC: lazy namespace changed beyond bounded scan cap during scan.\n"
        );

        return -1;
    }

    return 0;
}

static int address_list_count(
    ros_client_t *ros,
    const char *list,
    unsigned *count
)
{
    char query[192];

    snprintf(
        query,
        sizeof(query),
        "?list=%s",
        list
    );

    return gc_count_filtered_one(
        ros,
        "/ip/firewall/address-list/print",
        query,
        count
    );
}

static int lazy_id_exists(
    ros_client_t *ros,
    const char *comment,
    const char *id,
    int *exists
)
{
    char query[320];

    snprintf(
        query,
        sizeof(query),
        "?comment=%s",
        comment
    );

    id_find_ctx_t ctx = {
        .wanted_id = id,
        .found = 0
    };

    const char *cmd[] = {
        "/ip/firewall/mangle/print",
        "=.proplist=.id,comment",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            id_find_cb,
            &ctx
        ) < 0
    ) {
        return -1;
    }

    *exists = ctx.found;
    return 0;
}

static int remove_lazy_verified(
    ros_client_t *ros,
    const lazy_candidate_t *item,
    susanin_gc_stats_t *stats
)
{
    stats->actions++;

    (void)remove_id(
        ros,
        "/ip/firewall/mangle/remove",
        item->id
    );

    int exists = 0;

    if (
        lazy_id_exists(
            ros,
            item->comment,
            item->id,
            &exists
        ) < 0
    ) {
        return -1;
    }

    if (exists) return -1;

    stats->lazy_removed++;
    return 0;
}

static int process_lazy_kind(
    ros_client_t *ros,
    const lazy_collect_ctx_t *ctx,
    unsigned family_budget,
    susanin_gc_stats_t *stats,
    int verbose
)
{
    unsigned used = 0;

    for (
        size_t i = 0;
        i < ctx->count && used < family_budget;
        ++i
    ) {
        const lazy_candidate_t *item =
            &ctx->items[i];

        if (item->shape_unverifiable) {
            continue;
        }

        int remove_candidate = 0;

        if (!item->shape_valid) {
            /*
             * AUTO-AWG: P is Susanin-owned. An explicitly malformed rule
             * inside the exact active v0.12 namespace is orphan residue.
             */
            remove_candidate = 1;
        } else {
            char backing_list[128];

            make_state_list(
                backing_list,
                gc_state_name(item->parsed_kind),
                item->parsed_proto,
                item->parsed_port
            );

            unsigned count1 = 0;
            unsigned count2 = 0;

            if (
                address_list_count(
                    ros,
                    backing_list,
                    &count1
                ) < 0
            ) {
                return -1;
            }

            if (count1 == 0U) {
                /*
                 * Recheck immediately before deletion. This narrows the
                 * scheduler race. A remaining race can only remove the
                 * per-port marking path, which fails open to DIRECT.
                 */
                if (
                    address_list_count(
                        ros,
                        backing_list,
                        &count2
                    ) < 0
                ) {
                    return -1;
                }

                if (count2 == 0U) {
                    remove_candidate = 1;
                }
            }
        }

        if (!remove_candidate) continue;

        if (
            remove_lazy_verified(
                ros,
                item,
                stats
            ) < 0
        ) {
            return -1;
        }

        used++;

        if (verbose) {
            printf(
                "  GC lazy remove id=%s comment=\"%s\"\n",
                item->id,
                item->comment
            );
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Stale adaptive connection marks                                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    char id[64];
    char protocol[16];
    char dst[64];
    unsigned port;
    int valid;
    gc_kind_t kind;
} mark_candidate_t;

typedef struct {
    mark_candidate_t items[GC_MAX_MARKS];
    size_t count;
    int truncated;
    gc_kind_t kind;
} mark_collect_ctx_t;

static int mark_collect_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    mark_collect_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    if (ctx->count >= GC_MAX_MARKS) {
        ctx->truncated = 1;
        return 0;
    }

    mark_candidate_t *item =
        &ctx->items[ctx->count++];

    memset(
        item,
        0,
        sizeof(*item)
    );

    item->kind = ctx->kind;

    const char *id = ros_get_attr(s, ".id");
    const char *protocol = ros_get_attr(s, "protocol");
    const char *dst = ros_get_attr(s, "dst-address");
    const char *dst_port = ros_get_attr(s, "dst-port");

    snprintf(
        item->id,
        sizeof(item->id),
        "%s",
        id ? id : ""
    );

    snprintf(
        item->protocol,
        sizeof(item->protocol),
        "%s",
        protocol ? protocol : ""
    );

    snprintf(
        item->dst,
        sizeof(item->dst),
        "%s",
        dst ? dst : ""
    );

    unsigned port = 0;

    if (
        item->id[0] &&
        item->dst[0] &&
        (
            strcmp(item->protocol, "tcp") == 0 ||
            strcmp(item->protocol, "udp") == 0
        ) &&
        parse_port(
            dst_port,
            &port
        ) == 0
    ) {
        item->port = port;
        item->valid = 1;
    }

    return 0;
}

static int collect_mark_kind_once(
    ros_client_t *ros,
    gc_kind_t kind,
    mark_collect_ctx_t *out
)
{
    memset(
        out,
        0,
        sizeof(*out)
    );

    out->kind = kind;

    char query[128];

    snprintf(
        query,
        sizeof(query),
        "?connection-mark=%s",
        gc_mark_name(kind)
    );

    unsigned total = 0;

    if (
        gc_count_filtered_one(
            ros,
            "/ip/firewall/connection/print",
            query,
            &total
        ) < 0
    ) {
        return -1;
    }

    if (total > GC_MAX_MARKS) {
        fprintf(
            stderr,
            "Susanin GC: adaptive mark namespace exceeds bounded scan cap: %u > %u.\n",
            total,
            GC_MAX_MARKS
        );

        out->truncated = 1;
        return -1;
    }

    const char *cmd[] = {
        "/ip/firewall/connection/print",
        "=.proplist=.id,protocol,dst-address,dst-port",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            mark_collect_cb,
            out
        ) < 0
    ) {
        return -1;
    }

    if (out->truncated) {
        fprintf(
            stderr,
            "Susanin GC: adaptive mark namespace changed beyond bounded scan cap during scan.\n"
        );

        return -1;
    }

    return 0;
}

static int collect_mark_kind(
    ros_client_t *ros,
    gc_kind_t kind,
    mark_collect_ctx_t *out
)
{
    for (
        unsigned attempt = 1;
        attempt <= GC_MARK_SCAN_RETRIES;
        ++attempt
    ) {
        if (
            collect_mark_kind_once(
                ros,
                kind,
                out
            ) == 0
        ) {
            return 0;
        }

        if (attempt < GC_MARK_SCAN_RETRIES) {
            sleep_ms(100);
        }
    }

    return -1;
}

static int state_contains_address(
    ros_client_t *ros,
    const char *list,
    const char *address,
    int *found
)
{
    char list_query[192];
    char address_query[192];

    snprintf(
        list_query,
        sizeof(list_query),
        "?list=%s",
        list
    );

    snprintf(
        address_query,
        sizeof(address_query),
        "?address=%s",
        address
    );

    unsigned count = 0;

    if (
        gc_count_filtered_two_and(
            ros,
            "/ip/firewall/address-list/print",
            list_query,
            address_query,
            &count
        ) < 0
    ) {
        return -1;
    }

    *found = count > 0U;
    return 0;
}

typedef struct {
    char id[64];
} attempted_mark_t;

typedef struct {
    const attempted_mark_t *items;
    size_t count;
    int any_found;
} verify_marks_ctx_t;

static int verify_marks_cb(
    const ros_sentence_t *s,
    void *opaque
)
{
    verify_marks_ctx_t *ctx = opaque;

    if (!ctx || !ros_is_reply(s, "!re")) return 0;

    const char *id =
        ros_get_attr(
            s,
            ".id"
        );

    if (!id) return 0;

    for (size_t i = 0; i < ctx->count; ++i) {
        if (
            strcmp(
                id,
                ctx->items[i].id
            ) == 0
        ) {
            ctx->any_found = 1;
            break;
        }
    }

    return 0;
}

static int verify_mark_ids_absent_once(
    ros_client_t *ros,
    gc_kind_t kind,
    const attempted_mark_t *items,
    size_t count
)
{
    char query[128];

    snprintf(
        query,
        sizeof(query),
        "?connection-mark=%s",
        gc_mark_name(kind)
    );

    verify_marks_ctx_t ctx = {
        .items = items,
        .count = count,
        .any_found = 0
    };

    const char *cmd[] = {
        "/ip/firewall/connection/print",
        "=.proplist=.id",
        query
    };

    if (
        ros_command(
            ros,
            cmd,
            3,
            verify_marks_cb,
            &ctx
        ) < 0
    ) {
        return -1;
    }

    return
        ctx.any_found
            ? 1
            : 0;
}

static int verify_mark_ids_absent(
    ros_client_t *ros,
    gc_kind_t kind,
    const attempted_mark_t *items,
    size_t count
)
{
    for (
        unsigned attempt = 1;
        attempt <= GC_MARK_VERIFY_RETRIES;
        ++attempt
    ) {
        int rc =
            verify_mark_ids_absent_once(
                ros,
                kind,
                items,
                count
            );

        if (rc == 0) return 0;

        if (
            rc > 0 &&
            attempt == GC_MARK_VERIFY_RETRIES
        ) {
            return -1;
        }

        if (attempt < GC_MARK_VERIFY_RETRIES) {
            sleep_ms(100);
        }
    }

    return -1;
}

static int process_mark_kind(
    ros_client_t *ros,
    const mark_collect_ctx_t *ctx,
    unsigned family_budget,
    susanin_gc_stats_t *stats,
    int verbose
)
{
    attempted_mark_t attempted[GC_MARK_OK_BUDGET > GC_MARK_TEST_BUDGET
        ? GC_MARK_OK_BUDGET
        : GC_MARK_TEST_BUDGET];

    size_t attempted_count = 0;

    for (
        size_t i = 0;
        i < ctx->count &&
        attempted_count < family_budget;
        ++i
    ) {
        const mark_candidate_t *item =
            &ctx->items[i];

        /*
         * Missing live-conntrack properties are not proof of staleness.
         * Leave unverifiable connections alone.
         */
        if (!item->valid) continue;

        char backing_list[128];

        make_state_list(
            backing_list,
            gc_state_name(item->kind),
            item->protocol,
            item->port
        );

        int found1 = 0;
        int found2 = 0;

        if (
            state_contains_address(
                ros,
                backing_list,
                item->dst,
                &found1
            ) < 0
        ) {
            return -1;
        }

        if (found1) continue;

        /*
         * Recheck the exact tuple immediately before removing its stale
         * connection mark. Any residual race fails open to DIRECT.
         */
        if (
            state_contains_address(
                ros,
                backing_list,
                item->dst,
                &found2
            ) < 0
        ) {
            return -1;
        }

        if (found2) continue;

        stats->actions++;

        snprintf(
            attempted[attempted_count].id,
            sizeof(attempted[attempted_count].id),
            "%s",
            item->id
        );

        attempted_count++;

        /*
         * "no such item" is an expected expiry race for conntrack.
         * Fresh exact-mark verification below is authoritative.
         */
        (void)remove_id(
            ros,
            "/ip/firewall/connection/remove",
            item->id
        );

        if (verbose) {
            printf(
                "  GC stale mark remove id=%s mark=%s tuple=%s:%u dst=%s\n",
                item->id,
                gc_mark_name(item->kind),
                item->protocol,
                item->port,
                item->dst
            );
        }
    }

    if (attempted_count == 0U) return 0;

    if (
        verify_mark_ids_absent(
            ros,
            ctx->kind,
            attempted,
            attempted_count
        ) < 0
    ) {
        return -1;
    }

    stats->marked_cleared += (unsigned)attempted_count;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Public GC                                                                 */
/* ------------------------------------------------------------------------- */

int susanin_gc_run_once(
    ros_client_t *ros,
    const app_config_t *cfg,
    susanin_gc_stats_t *stats,
    int verbose
)
{
    if (!ros || !cfg || !stats) return -1;

    memset(
        stats,
        0,
        sizeof(*stats)
    );

    if (verbose) {
        printf(
            "=== SUSANIN GC v%s ===\n",
            SUSANIN_VERSION
        );

        printf(
            "Bounds: lazy-test<=%u lazy-ok<=%u marked-test<=%u marked-ok<=%u total<=%u\n",
            GC_LAZY_TEST_BUDGET,
            GC_LAZY_OK_BUDGET,
            GC_MARK_TEST_BUDGET,
            GC_MARK_OK_BUDGET,
            SUSANIN_GC_TOTAL_BUDGET
        );
    }

    int gate =
        gc_normal_runtime_gate(
            ros,
            cfg,
            verbose
        );

    if (gate > 0) {
        stats->skipped = 1;

        if (verbose) {
            printf(
                "GC result: SKIP — RouterOS left unchanged.\n"
            );
        }

        return 0;
    }

    if (gate < 0) {
        return gc_fail_safe(
            ros,
            "normal v0.12 ownership was proven but runtime gate became unverifiable"
        );
    }

    /*
     * 1. Persistent lazy per-port rules.
     */
    lazy_collect_ctx_t lazy_test;
    lazy_collect_ctx_t lazy_ok;

    if (
        collect_lazy_kind(
            ros,
            GC_KIND_TEST,
            &lazy_test
        ) < 0 ||
        collect_lazy_kind(
            ros,
            GC_KIND_OK,
            &lazy_ok
        ) < 0
    ) {
        return gc_fail_safe(
            ros,
            "lazy rule scan failed"
        );
    }

    if (lazy_test.truncated) stats->scans_truncated++;
    if (lazy_ok.truncated) stats->scans_truncated++;

    if (
        process_lazy_kind(
            ros,
            &lazy_test,
            GC_LAZY_TEST_BUDGET,
            stats,
            verbose
        ) < 0 ||
        process_lazy_kind(
            ros,
            &lazy_ok,
            GC_LAZY_OK_BUDGET,
            stats,
            verbose
        ) < 0
    ) {
        return gc_fail_safe(
            ros,
            "lazy rule removal post-condition failed"
        );
    }

    /*
     * 2. Safely identifiable stale TEST/OK connection marks.
     *
     * The exact per-port/per-destination TEST or OK address-list entry is
     * the authoritative backing state. Without it, the mark is stale.
     */
    mark_collect_ctx_t mark_test;
    mark_collect_ctx_t mark_ok;

    if (
        collect_mark_kind(
            ros,
            GC_KIND_TEST,
            &mark_test
        ) < 0 ||
        collect_mark_kind(
            ros,
            GC_KIND_OK,
            &mark_ok
        ) < 0
    ) {
        return gc_fail_safe(
            ros,
            "adaptive marked-connection scan did not converge"
        );
    }

    if (mark_test.truncated) stats->scans_truncated++;
    if (mark_ok.truncated) stats->scans_truncated++;

    if (
        process_mark_kind(
            ros,
            &mark_test,
            GC_MARK_TEST_BUDGET,
            stats,
            verbose
        ) < 0 ||
        process_mark_kind(
            ros,
            &mark_ok,
            GC_MARK_OK_BUDGET,
            stats,
            verbose
        ) < 0
    ) {
        return gc_fail_safe(
            ros,
            "stale adaptive marked-connection post-condition failed"
        );
    }

    if (
        stats->actions >
        SUSANIN_GC_TOTAL_BUDGET
    ) {
        return gc_fail_safe(
            ros,
            "internal mutation budget invariant violated"
        );
    }

    if (
        verbose ||
        stats->actions > 0U ||
        stats->scans_truncated > 0U
    ) {
        printf(
            "GC summary: lazy=%u marked=%u actions=%u/%u truncated-scans=%u\n",
            stats->lazy_removed,
            stats->marked_cleared,
            stats->actions,
            SUSANIN_GC_TOTAL_BUDGET,
            stats->scans_truncated
        );

        printf(
            "GC result: PASS\n"
        );
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Daemon tick                                                               */
/* ------------------------------------------------------------------------- */

int susanin_gc_daemon_tick(void)
{
    int lock_fd = -1;

    int lock_rc =
        susanin_runtime_lock_try(
            &lock_fd
        );

    if (lock_rc > 0) {
        /*
         * Another lifecycle operation is active.
         * A daemon GC tick is optional and can safely wait for the next
         * five-minute interval.
         */
        return 0;
    }

    if (lock_rc < 0) {
        fprintf(
            stderr,
            "Susanin GC daemon: runtime lock unavailable; retrying later.\n"
        );

        return -1;
    }

    int rc = -1;

    app_config_t cfg;

    if (
        config_load(
            &cfg
        ) < 0
    ) {
        fprintf(
            stderr,
            "Susanin GC daemon: local configuration unavailable; retrying later.\n"
        );

        goto out_unlock;
    }

    ros_client_t ros;

    if (
        ros_connect(
            &ros,
            cfg.host,
            cfg.port
        ) < 0
    ) {
        fprintf(
            stderr,
            "Susanin GC daemon: RouterOS API connection failed; retrying later.\n"
        );

        goto out_unlock;
    }

    if (
        ros_login(
            &ros,
            cfg.user,
            cfg.password
        ) < 0
    ) {
        fprintf(
            stderr,
            "Susanin GC daemon: RouterOS API authentication failed; retrying later.\n"
        );

        ros_close(
            &ros
        );

        goto out_unlock;
    }

    susanin_gc_stats_t stats;

    rc =
        susanin_gc_run_once(
            &ros,
            &cfg,
            &stats,
            0
        );

    ros_close(
        &ros
    );

out_unlock:
    susanin_runtime_lock_release(
        lock_fd
    );

    return rc;
}
