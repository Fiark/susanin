#ifndef SUSANIN_GC_H
#define SUSANIN_GC_H

#include "config.h"
#include "routeros_api.h"

#define SUSANIN_GC_INTERVAL_SECONDS 300U
#define SUSANIN_GC_TOTAL_BUDGET 12U

/*
 * WATCH / TEST / OK / COOLDOWN and DIRECT1 / AWG1 / RECHECK are created by
 * current v0.12 RouterOS templates only with finite timeouts. RouterOS owns
 * expiry of that timed state.
 *
 * The controller therefore never performs a broad address-list sweep.
 * Persistent GC targets are lazy mangle rules and stale adaptive marks.
 */

typedef struct {
    unsigned lazy_removed;
    unsigned marked_cleared;
    unsigned actions;
    unsigned scans_truncated;
    int skipped;
} susanin_gc_stats_t;

/*
 * Run one bounded runtime-GC iteration.
 *
 * GC mutates RouterOS only after exact current v0.12 desired production
 * ownership has been proven. Stable v0.11.x production therefore causes a
 * safe SKIP.
 */
int susanin_gc_run_once(
    ros_client_t *ros,
    const app_config_t *cfg,
    susanin_gc_stats_t *stats,
    int verbose
);

/*
 * One daemon iteration:
 *
 * load config -> connect -> authenticate -> run bounded GC -> disconnect.
 */
int susanin_gc_daemon_tick(void);

/*
 * Serialize runtime-mutating controller operations inside one Susanin
 * container/data namespace.
 *
 * Return:
 *   0  lock acquired
 *   1  another controller operation currently owns the lock
 *  -1  lock operation failed
 */
int susanin_runtime_lock_try(
    int *fd_out
);

void susanin_runtime_lock_release(
    int fd
);

#endif
