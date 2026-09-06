# Susanin v0.12.0-dev3 — accuracy profile state machines

Base: accepted `v0.12.0-dev2`.

dev3 changes evidence requirements only. The accepted dev2 routing target,
port-aware identity, lazy mangle rules, VPN Direct priority, fail-open model,
RouterOS API framing and connection cleanup behavior remain the baseline.

## Adaptive identity

Every decision remains scoped to:

    protocol + destination IPv4 + destination port

No profile may widen a decision to the whole destination IP.

## Existing evidence signals

dev3 does not change the detector thresholds accepted in dev2.

Strong DIRECT evidence continues to come from the existing FAST/SOFT
detectors, including the accepted TCP SYN, TCP close/stall and UDP failure
criteria.

Healthy or failed AWG evidence continues to use the accepted JUDGE
thresholds.

The accuracy profile changes only how many independent pieces of evidence are
required and which transition happens next.

## Profile activation

`accuracy_profile` is already persistent configuration.

Supported values:

- `fast`
- `middle`
- `slow`

The selected profile is rendered into the RouterOS scripts.

Changing the local config alone does not mutate RouterOS. A new profile takes
effect only through the normal generated-script lifecycle (`stage` /
`promote`, or installation).

## Temporary evidence state

dev3 adds three lazily used per-protocol/per-port address-list families:

    auto_awg_direct1_<proto>_<port>
    auto_awg_awg1_<proto>_<port>
    auto_awg_recheck_<proto>_<port>

Examples:

    auto_awg_direct1_tcp_443
    auto_awg_awg1_tcp_443
    auto_awg_recheck_tcp_443

They are tuple-scoped because the address inside each list is the destination
IPv4.

Default evidence timeout:

- DIRECT evidence #1: 2 minutes
- AWG evidence #1: 2 minutes
- SLOW DIRECT re-check: 2 minutes

These lists are transient evidence only. They are never interpreted as an OK
routing decision.

The existing lists keep their dev2 roles:

    auto_awg_watch_<proto>_<port>
    auto_awg_test_<proto>_<port>
    auto_awg_ok_<proto>_<port>
    auto_awg_cooldown_<proto>_<port>

`WATCH` remains detector-internal transient state; it is not reused as a
profile evidence counter.

## FAST

FAST preserves accepted dev2 behavior.

State machine:

    DIRECT
      |
      | one strong DIRECT failure
      v
    TEST
      |
      | one healthy AWG connection
      v
    OK

A failed AWG TEST enters the existing cooldown path.

No `direct1`, `awg1`, or `recheck` evidence is required.

## MIDDLE

MIDDLE requires two independent DIRECT failures and two independent healthy
AWG connections.

State machine:

    DIRECT
      |
      | DIRECT failure #1
      v
    DIRECT1
      |
      | independent DIRECT failure #2
      v
    TEST
      |
      | healthy AWG connection #1
      v
    TEST + AWG1
      |
      | independent healthy AWG connection #2
      v
    OK

Rules:

1. DIRECT failure #1 adds `auto_awg_direct1_*` and removes the failed
   connection so the next evidence must come from a new connection.
2. DIRECT failure #2 removes `direct1`, creates/uses the exact dev2 lazy
   rules, enters TEST and removes the failed connection.
3. The first healthy TEST connection adds `auto_awg_awg1_*`, keeps TEST
   active and removes that marked connection.
4. A later independent healthy TEST connection while `awg1` is present
   confirms OK.
5. A failed AWG TEST clears temporary profile evidence and follows the
   existing TEST failure/cooldown path.
6. Evidence expiry never produces an AWG decision.

## SLOW

SLOW adds a real DIRECT re-check between the first and second healthy AWG
tests.

SNI is deliberately not part of dev3. dev4 will add SNI as an additional
confidence signal without weakening this state machine.

State machine:

    DIRECT
      |
      | DIRECT failure #1
      v
    DIRECT1
      |
      | independent DIRECT failure #2
      v
    TEST (AWG #1)
      |
      | healthy AWG connection #1
      v
    AWG1 + DIRECT-RECHECK
      |
      | a new strong DIRECT failure
      v
    TEST + AWG1 (AWG #2)
      |
      | healthy AWG connection #2
      v
    OK

Rules:

1. The first two DIRECT evidence transitions are identical to MIDDLE.
2. The first healthy AWG TEST does NOT confirm the tuple.
3. Instead it:
   - adds `auto_awg_awg1_*`;
   - removes TEST;
   - adds `auto_awg_recheck_*`;
   - removes the marked AWG connection.
4. While `recheck` exists, new traffic is DIRECT.
5. Only a new strong DIRECT failure during that window:
   - removes `recheck`;
   - keeps `awg1`;
   - re-enters TEST;
   - removes the failed DIRECT connection.
6. The next independent healthy AWG TEST while `awg1` exists confirms OK.
7. If DIRECT does not produce another strong failure before the evidence
   expires, no OK decision is made and the tuple naturally returns to normal
   DIRECT behavior.
8. Any failed AWG TEST clears transient profile evidence and uses the
   existing cooldown behavior.

This makes SLOW fail-safe: lack of evidence can only prevent confirmation; it
cannot force AWG.

## VPN Direct

VPN Direct keeps absolute priority in every profile.

A destination currently covered by `vpn_direct` must not accumulate new
profile evidence and must not enter TEST or OK due to adaptive logic.

## Routing-target failure

When HEALTH declares the selected routing target down:

- adaptive mangle remains fail-open as in dev2;
- TEST/marked connections are cleared;
- `direct1`, `awg1`, and `recheck` transient evidence is cleared;
- confirmed OK state remains governed by the accepted HEALTH recovery model.

No profile evidence may keep traffic pinned to a failed target.

## Promotion and profile migration

dev3 treats promotion as a fail-open adaptive-state compatibility boundary.

Before the promoted scripts resume, promotion must clear:

- dev2 lazy per-port rules;
- all per-port WATCH/TEST/OK/COOLDOWN state;
- dev3 `direct1` / `awg1` / `recheck` evidence;
- adaptive TEST/OK connection marks;
- legacy v0.11.x IP-only adaptive state.

This intentionally forgets learned routing decisions during a dev3
promotion/profile switch.

Selective migration of compatible state is deferred to dev4.

## Rollback

Rollback must clear all dev3 evidence in addition to the accepted dev2
runtime cleanup, then restore the exact backed-up production scripts.

If cleanup or source restoration cannot be verified, the existing fail-open
behavior remains mandatory.

## Acceptance

Each profile is accepted independently on RouterOS 7.23.3 / ARM64.

FAST must reproduce the already accepted dev2 functional path.

MIDDLE must prove:

1. one DIRECT failure is insufficient;
2. a second independent DIRECT failure creates TEST;
3. one healthy AWG connection is insufficient for OK;
4. a second independent healthy AWG connection creates OK;
5. other protocol/port tuples remain isolated.

SLOW must prove:

1. one DIRECT failure is insufficient;
2. the second enters first TEST;
3. first AWG success returns the tuple to DIRECT re-check;
4. no second DIRECT failure means no OK;
5. a second DIRECT failure enters final TEST;
6. second AWG success creates OK;
7. other protocol/port tuples remain isolated.

All tests must end with exact stable production recovery.
