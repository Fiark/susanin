#ifndef SUSANIN_STATE_CLEANUP_H
#define SUSANIN_STATE_CLEANUP_H

#include "routeros_api.h"

typedef struct {
    unsigned legacy_entries;
    unsigned port_entries;
    unsigned lazy_rules;
    unsigned marked_connections;
} susanin_cleanup_stats_t;

/*
 * Clear v0.11.5 IP-only learned state and TEST/OK marked connections.
 *
 * Used during successful dev2 promotion. Old IP-only state must never
 * become an implicit all-ports decision.
 */
int susanin_cleanup_legacy_state(
    ros_client_t *ros,
    susanin_cleanup_stats_t *stats
);

/*
 * Clear all adaptive runtime state owned by dev2:
 *
 * - lazy AUTO-AWG: P ... mangle rules;
 * - per-protocol/per-port state lists;
 * - legacy IP-only state lists;
 * - TEST/OK marked connections.
 *
 * Used for rollback and fresh-install residue cleanup.
 */
int susanin_cleanup_dev2_runtime(
    ros_client_t *ros,
    susanin_cleanup_stats_t *stats
);

/*
 * Emergency fail-open fallback.
 *
 * Disable all Susanin AUTO-AWG mangle rules. VPN Direct and independent
 * AWG infrastructure are not part of this namespace.
 */
int susanin_disable_adaptive_mangle(
    ros_client_t *ros
);

#endif
