# Susanin v0.12.0-dev4

## Baseline

DEV4 starts from the fully accepted DEV3 line.

Accepted executable DEV3:

    96277e3e6e2f38afe6cf8fb7124ae66c8c72ca87

DEV3 documentation/acceptance HEAD:

    a32328da1dc9d7613294cf0c7a079813b24984f0

Accepted final DEV2 foundation:

    403ca95699d108daa72f1e735935007be78becc3

DEV4 must preserve all accepted DEV3 behavior unless an explicit DEV4
acceptance test proves a compatible change.

## Invariants

Adaptive identity remains:

    protocol + destination IPv4 + destination port

The decision for one tuple must never become a wildcard decision for the
whole destination IP.

VPN Direct keeps absolute priority.

Routing-table mode remains table-native.

Susanin never owns or deletes the independent AWG infrastructure:

    awg-proxy-arm64
    veth-awg-proxy
    wg-awg-proxy
    r_to_awg
    AWG selected traffic masquerade

Failure or uncertainty must remain fail-open to DIRECT.

RouterOS operation remains strict IPv4-only for v0.12.

FAST / MIDDLE / SLOW accepted in DEV3 remain the routing-engine baseline.

## Explicitly out of scope for v0.12

SNI / TLS-hostname-assisted routing is not part of v0.12.

The DEV4 RouterOS tls-host experiments proved matching capability but no
SNI-dependent product behavior was implemented.

No SNI, DNS-derived hostname evidence, TLS parser, packet sniffer or TZSP
collector will be added to the v0.12 release path.

This may be reconsidered in a later release without blocking v0.12.

## DEV4 milestones

### D4.1 Migration hardening

Promotion, rollback and profile changes are compatibility boundaries.

Requirements:

- pause adaptive schedulers before mutation;
- wait managed RouterOS script jobs idle;
- verify staged source before production mutation;
- keep exact rollback backups;
- clear incompatible adaptive runtime before scheduler resume;
- preserve state only where compatibility is explicitly proven;
- use narrow/specialized RouterOS reads;
- tolerate proven transient conntrack races;
- preserve API trap-drain behavior;
- verify every cleanup post-condition;
- any uncertainty must leave adaptive routing fail-open to DIRECT;
- independent AWG infrastructure must never be modified.

The v0.12 migration policy is deliberately conservative:

    incompatible adaptive state is forgotten

rather than guessed or widened.

### D4.2 Bounded runtime GC

Implement bounded garbage collection for orphaned adaptive runtime objects.

Targets include:

- orphaned lazy `AUTO-AWG: P ...` rules;
- WATCH / TEST / OK / COOLDOWN tuple state;
- DIRECT1 / AWG1 / RECHECK profile evidence;
- safely identifiable stale adaptive marked connections.

GC must:

- have bounded work per invocation;
- avoid unbounded RouterOS full-table loops;
- never touch legacy production state while v0.11.5 is active;
- never touch VPN Direct;
- never touch independent AWG infrastructure;
- use narrow RouterOS queries;
- tolerate object expiry races;
- leave traffic DIRECT on uncertainty.

GC is cleanup, not a routing-decision engine.

### D4.3 Fail-open soak

Long-running acceptance must cover:

- ordinary DIRECT traffic;
- FAST / MIDDLE / SLOW;
- learned TEST and OK traffic;
- routing-target loss and recovery;
- API traps;
- conntrack churn;
- repeated stage / promote / rollback;
- repeated FAST / MIDDLE / SLOW profile changes;
- reboot and startup behavior;
- bounded RouterOS CPU/API load;
- bounded container memory;
- no long-term growth of orphaned adaptive objects;
- zero impact on independent AWG infrastructure.

### D4.4 Release closure

Before stable v0.12.0:

- build final linux/arm64 image;
- verify exact executable/source fingerprints;
- validate clean install;
- validate upgrade from stable v0.11.5;
- validate rollback;
- validate reboot/startup;
- verify VPN Direct;
- verify FAST / MIDDLE / SLOW;
- verify bounded GC;
- complete fail-open soak;
- restore exact reference-router production state;
- update user documentation;
- update changelog and release notes;
- create stable v0.12.0 tag only after all gates pass.

## Acceptance order

1. Migration compatibility matrix.
2. Migration fault/recovery tests.
3. Bounded runtime GC implementation.
4. GC high-volume stress.
5. Fail-open soak.
6. Final ARM64 release candidate.
7. Install / upgrade / rollback / reboot acceptance.
8. Exact final production recovery.
9. Stable v0.12.0 release.
