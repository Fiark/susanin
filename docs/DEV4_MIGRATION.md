# Susanin v0.12.0-dev4 — migration hardening

## Goal

Prove that every supported v0.12 transition is transactional enough for the
reference RouterOS 7.23.3 / ARM64 environment and fails open to DIRECT when a
post-condition cannot be verified.

Migration is not required to preserve learned routing decisions.

The safe default is:

    clear incompatible adaptive runtime
    -> resume from DIRECT
    -> learn again

## Existing accepted foundation

DEV2/DEV3 already established:

- exact stage verification;
- four managed production scripts;
- persistent rollback backups;
- managed scheduler pause/resume;
- wait-for-script-jobs-idle;
- RouterOS API trap reply draining;
- retry handling for live conntrack scan races;
- cleanup of legacy IP-only state;
- cleanup of port-aware tuple state;
- cleanup of DEV3 evidence state;
- cleanup of adaptive TEST/OK connection marks;
- fail-open behavior on cleanup failure.

DEV4 must preserve these properties.

## Runtime compatibility boundary

The following state families are incompatible across a conservative v0.12
migration/profile switch unless explicitly proven otherwise.

Legacy v0.11.x:

    auto_awg_watch_tcp
    auto_awg_test_tcp
    auto_awg_ok_tcp
    auto_awg_cooldown_tcp
    auto_awg_watch_udp
    auto_awg_test_udp
    auto_awg_ok_udp
    auto_awg_cooldown_udp

Port-aware DEV2/DEV3:

    auto_awg_watch_<proto>_<port>
    auto_awg_test_<proto>_<port>
    auto_awg_ok_<proto>_<port>
    auto_awg_cooldown_<proto>_<port>

DEV3 profile evidence:

    auto_awg_direct1_<proto>_<port>
    auto_awg_awg1_<proto>_<port>
    auto_awg_recheck_<proto>_<port>

Lazy dataplane rules:

    AUTO-AWG: P <proto> <port> OK
    AUTO-AWG: P <proto> <port> TEST

Adaptive connection marks:

    auto-awg-test-conn
    auto-awg-ok-conn

## Mandatory transition matrix

### M1 — stable v0.11.5 -> v0.12 FAST

Preload controlled legacy WATCH/TEST/OK/COOLDOWN state and adaptive marked
connections.

Promotion must:

- pause schedulers;
- wait jobs idle;
- install exact desired sources;
- clear legacy IP-only state;
- clear adaptive marked connections;
- leave no DEV2/DEV3 runtime residue;
- restore original scheduler enabled/disabled state;
- preserve independent AWG infrastructure.

### M2 — FAST -> MIDDLE

Preload controlled tuple state:

- WATCH;
- TEST;
- OK;
- COOLDOWN;
- lazy OK/TEST rules;
- adaptive marked connections.

Profile promotion must remove incompatible runtime before resume.

No previous FAST decision may be interpreted as MIDDLE evidence.

### M3 — MIDDLE -> SLOW

Preload:

- DIRECT1;
- AWG1;
- TEST;
- OK;
- lazy rules;
- marked connections.

All incompatible state must be cleared.

No MIDDLE evidence may be silently reused as SLOW re-check evidence.

### M4 — SLOW -> FAST

Preload:

- DIRECT1;
- AWG1;
- RECHECK;
- TEST/OK;
- lazy rules;
- marked connections.

FAST must resume from a clean adaptive runtime.

### M5 — same-profile re-promotion

Promoting the exact same generated sources again must remain safe.

It may conservatively forget adaptive runtime.

It must not:

- duplicate managed scripts;
- duplicate schedulers;
- accumulate backup objects;
- change AWG infrastructure;
- leave stale stage objects after success.

### M6 — profile-switch repetition

Run a bounded sequence such as:

    FAST -> MIDDLE -> SLOW -> FAST -> MIDDLE

Every transition must finish with:

- exactly four managed production scripts;
- exactly four managed production schedulers;
- zero incompatible runtime residue;
- valid rollback backups for the immediately previous production sources;
- unchanged AWG infrastructure.

### M7 — live conntrack churn during migration

Generate controlled short-lived TEST/OK marked connections while migration
cleanup runs.

Known RouterOS 7.23.3 `no such item` races may occur.

Required result:

- API framing remains synchronized;
- cleanup retries remain bounded;
- post-condition reaches zero incompatible adaptive marks;
- promotion either succeeds completely or restores/fails open.

### M8 — stage mismatch preflight

Deliberately make one staged source differ from generated desired source.

Promotion must stop before:

- production source mutation;
- scheduler mutation;
- adaptive runtime cleanup.

### M9 — rollback after successful promotion

After a successful promotion:

- rollback must pause schedulers;
- wait jobs idle;
- clear current adaptive runtime;
- restore the exact four backed-up production sources;
- restore saved scheduler states;
- remove no independent AWG object.

### M10 — cleanup failure / unverifiable post-condition

A controlled fault must demonstrate that an unverifiable cleanup does not
resume adaptive routing as if migration succeeded.

Required safety result:

    adaptive mangle disabled / schedulers paused as needed
    -> traffic fails open to DIRECT
    -> operator-visible failure

The exact fault-injection method must not endanger the independent AWG
container/interface/table/NAT.

## Global post-conditions

Every migration acceptance case checks:

Production topology:

    managed scripts = 4
    managed schedulers = 4

Independent AWG:

    wg-awg-proxy = 1
    r_to_awg = 1
    AWG selected traffic masquerade = 1

Reference platform:

    disable-ipv6 = true

No unexpected test residue:

    temporary raw rules = 0
    temporary mangle rules = 0
    temporary LAN membership = 0
    temporary test scripts = 0

When a case ends in stable v0.11.5 reference production:

    source lengths = 4186 / 4041 / 8075 / 6122

## Acceptance strategy

Do not combine the entire matrix into one giant RouterOS harness.

Run small, independently recoverable cases.

Every write-mode case starts with an exact production/AWG hard gate and ends
with an exact recovery gate.

If a test harness itself is ambiguous, do not change product code until the
RouterOS post-condition identifies a real product failure.
