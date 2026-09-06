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

## DEV4 milestones

### D4.1 SNI observability

First implement only an observability/capability probe.

SNI must initially have no effect on routing decisions.

The probe must establish what RouterOS 7.23.3 can reliably observe from
ordinary TLS traffic.

Absence of observable SNI is neutral.

SNI must never be required for a valid DEV3 decision.

Known no-signal or incomplete-signal cases include:

- fragmented TLS ClientHello;
- QUIC;
- ECH;
- applications not using TLS SNI;
- traffic whose hostname is otherwise unavailable.

Only after observability is proven may SNI become an additional confidence
signal.

### D4.2 SNI-assisted confidence

SNI may strengthen confidence but may not:

- override VPN Direct;
- widen tuple identity;
- force AWG by itself;
- weaken FAST/MIDDLE/SLOW evidence requirements;
- interpret missing SNI as failure.

Without usable SNI the state machine must behave exactly like accepted DEV3.

### D4.3 Migration hardening

Promotion and profile changes are compatibility boundaries.

Requirements:

- pause adaptive schedulers;
- wait managed jobs idle;
- clear incompatible transient state;
- preserve state only where compatibility is explicitly proven;
- use narrow/specialized RouterOS reads;
- tolerate known transient conntrack churn;
- preserve the accepted API trap-drain behavior;
- any unverifiable state leaves adaptive routing fail-open.

### D4.4 Bounded runtime GC

Implement bounded garbage collection for orphaned adaptive state.

Candidates include:

- orphaned lazy AUTO-AWG: P rules;
- WATCH / TEST / OK / COOLDOWN lists;
- DIRECT1 / AWG1 / RECHECK evidence;
- stale adaptive marked connections where safely identifiable.

GC must:

- have bounded work per run;
- avoid unbounded full-table loops;
- never touch VPN Direct;
- never touch independent AWG infrastructure;
- use narrow RouterOS queries;
- fail open on uncertainty.

### D4.5 Fail-open soak

Long-running acceptance must cover:

- ordinary DIRECT traffic;
- FAST / MIDDLE / SLOW;
- target failure and recovery;
- API traps;
- conntrack churn;
- repeated stage/promote/rollback;
- repeated profile changes;
- reboot/startup behavior;
- bounded CPU/API load;
- zero impact on independent AWG infrastructure.

## Acceptance order

1. SNI capability probe with zero routing mutation.
2. Decision-neutral SNI observability.
3. SNI-assisted confidence integration.
4. Migration compatibility tests.
5. Bounded GC stress.
6. Long fail-open soak.
7. Final ARM64 acceptance.
8. Exact production recovery verification.
