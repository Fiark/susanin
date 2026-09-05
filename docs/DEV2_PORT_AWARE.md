# Susanin v0.12.0-dev2 port-aware adaptive state

## Identity

Adaptive learning identity is:

    protocol + destination IPv4 + destination port

Examples:

    tcp / 203.0.113.10 / 443
    tcp / 203.0.113.10 / 80
    udp / 203.0.113.10 / 443

These are three independent adaptive identities.

## Required behavior

A decision for TCP/443 must never automatically route TCP/80 or UDP/443.

VPN Direct remains higher priority than every adaptive state.

Existing connections remain pinned with connection marks.

## RouterOS state model

dev2 will use lazy per-protocol/per-port buckets.

Initial naming contract:

    auto_awg_watch_tcp_443
    auto_awg_test_tcp_443
    auto_awg_ok_tcp_443
    auto_awg_cooldown_tcp_443

and equivalently for UDP and other ports.

Rules for a port are created only when that protocol/port is observed.

Unused port buckets/rules must later be garbage-collected.

## Compatibility

v0.11.5 IP-only adaptive lists are legacy state.

dev2 must not silently interpret an old IP-only OK entry as permission
for every destination port.

Migration must be fail-open:

- legacy learned state may be ignored/reset;
- DIRECT remains the safe fallback;
- no old IP-only decision may become a wildcard port decision.

## FAST profile

dev2 changes identity granularity only.

FAST evidence thresholds should otherwise remain behaviorally equivalent
to the current accepted baseline.

MIDDLE/SLOW evidence changes belong to dev3.

## VPN Direct

Before creating WATCH/TEST/OK state, the destination must be checked
against `vpn_direct`.

VPN Direct always wins.

## IPv4

All adaptive identities are IPv4 only.

No IPv6 adaptive state or rules are created.

## Source-mapped dev2 design freeze

The current v0.11.5/dev1 adaptive data-plane was mapped before the first
dev2 implementation patch.

### Current transition ownership

FAST:

- TCP SYN failure -> TEST;
- early TCP close -> TEST;
- UDP/443 QUIC no-reply -> TEST;
- failed connection is removed after TEST is prepared.

SOFT/DETECT:

- TCP hard stall -> TEST;
- TCP late stall -> WATCH -> TEST;
- generic UDP no-reply -> TEST;
- UDP/443 late stall -> WATCH -> TEST;
- failed connection is removed after TEST is prepared.

JUDGE:

- TEST + accepted VPN evidence -> OK for 6h;
- TEST + failed VPN evidence -> COOLDOWN for 5m;
- confirmed OK + VPN failure -> COOLDOWN for 30s;
- healthy confirmed state is refreshed when required.

HEALTH:

- probes the selected routing table;
- after two misses disables AUTO-AWG rules;
- removes TEST/OK marked connections;
- clears transient WATCH/TEST state;
- falls open to DIRECT;
- on recovery enables AUTO-AWG and reconnects confirmed destinations.

FAST evidence thresholds remain unchanged in dev2.
Only adaptive identity changes.

### Port-aware state identity

The state key is:

    protocol + destination IPv4 + destination port

State-list names are:

    auto_awg_watch_<proto>_<port>
    auto_awg_test_<proto>_<port>
    auto_awg_ok_<proto>_<port>
    auto_awg_cooldown_<proto>_<port>

For example:

    auto_awg_watch_tcp_443
    auto_awg_test_tcp_443
    auto_awg_ok_tcp_443
    auto_awg_cooldown_tcp_443

TCP/443, TCP/80 and UDP/443 are independent identities.

### Lazy mangle design

The existing common connection marks remain:

    auto-awg-test-conn
    auto-awg-ok-conn

The existing common route-mark rules remain.

When a protocol/port reaches TEST for the first time, RouterOS creates at
most two lazy mark-connection rules:

    AUTO-AWG: P tcp 443 OK
    AUTO-AWG: P tcp 443 TEST

Equivalent UDP rules use `udp`.

Each rule matches:

- chain=prerouting;
- exact protocol;
- exact destination port;
- exact per-port TEST or OK address-list;
- connection-mark=no-mark;
- connection-state=new;
- dst-address-type=!local;
- the configured LAN interface-list;
- action=mark-connection;
- the common TEST or OK connection mark;
- passthrough=yes.

Lazy mark rules are inserted before:

    AUTO-AWG: L2 route confirmed

This ordering is mandatory.

The clean reconnect must receive its connection mark before the common
route-mark rule is evaluated in the same prerouting pass.

If the placement anchor is absent, Susanin must fail open and must not move
the tuple into TEST.

The lazy rule pair must be ensured before adding TEST state and before
removing the failed DIRECT connection.

### Fixed compatibility rules

The existing eight fixed AUTO-AWG rules remain during dev2.

The four legacy IP-only mark-connection rules become inert because dev2
does not populate their old lists.

Keeping them avoids an unnecessary install topology rewrite and preserves
the existing HEALTH sentinel:

    AUTO-AWG: L1 mark test

The common route rules and router DNS rules remain active.

### VPN Direct precedence

FAST and SOFT/DETECT must check `vpn_direct` before creating WATCH or TEST.

A destination currently present in `vpn_direct` must not acquire new
adaptive state and its connection must not be removed by adaptive learning.

JUDGE must not promote a TEST tuple to OK while that destination is in
`vpn_direct`.

The existing:

    SUSANIN: VPN Direct bypass

remains before the first AUTO-AWG rule and therefore has absolute packet
path priority.

Lazy rules intentionally keep the AUTO-AWG prefix so VPN Direct ordering
and HEALTH global enable/disable continue to work.

### HEALTH port-aware requirements

Target-down cleanup must remove per-port WATCH and TEST state.

Confirmed OK state remains cached across a temporary target outage.

Recovery must be tuple-aware.

For each DIRECT/no-mark LAN connection HEALTH derives:

    auto_awg_ok_<protocol>_<destination-port>

and removes the connection only if its destination IPv4 exists in that
exact list.

Therefore an OK decision for TCP/443 cannot tear down TCP/80 to the same IP.

### Fail-open legacy migration

The old learned lists:

    auto_awg_watch_tcp
    auto_awg_test_tcp
    auto_awg_ok_tcp
    auto_awg_cooldown_tcp
    auto_awg_watch_udp
    auto_awg_test_udp
    auto_awg_ok_udp
    auto_awg_cooldown_udp

must never be interpreted as wildcard-port state.

Successful dev2 promotion must:

1. pause the four managed schedulers;
2. wait until managed jobs are idle;
3. update and verify all four dev2 script sources;
4. clear all eight exact legacy learned lists;
5. remove auto-awg-test-conn and auto-awg-ok-conn connections;
6. resume schedulers only after successful verification and cleanup.

Fresh install performs the same legacy-state cleanup before enabling the
data-plane.

This cleanup is intentionally fail-open to DIRECT.

It must not touch independent AWG interfaces, routing-table contents or
externally managed NAT.

### Rollback requirement

After dev2 runs, per-port lists and lazy mangle rules may exist even if the
four script sources are later restored.

Rollback must therefore also:

- pause the managed schedulers;
- remove TEST/OK marked connections;
- remove dev2 per-port WATCH/TEST/OK/COOLDOWN entries;
- remove only mangle rules with the dev2 `AUTO-AWG: P ` prefix;
- restore and verify the four backed-up production scripts;
- restore scheduler states.

The fixed eight compatibility rules remain.

After rollback the old lists are empty and the stable v0.11.5 logic can
relearn safely from DIRECT.

### Apply reconciliation requirement

The current apply implementation stores only a bounded AUTO-AWG mangle
snapshot.

That design is not safe after lazy per-port rules exist because the number
of AUTO-AWG rules is no longer bounded by the fixed installation set.

Fixed managed rules must be validated by specialized exact-comment RouterOS
queries.

Dynamic per-port rules are allowed additional objects and must not hide fixed
rules from reconciliation.

This also follows the RouterOS 7.23.3 correctness lesson: prefer several
specialized queries over one large consolidated query when correctness
depends on dynamic RouterOS properties.

### Status and garbage-collection foundation

Status should report fixed installation health separately from per-port
bucket/state counts.

Normal status output does not need to print every lazy rule.

Stable namespaces are the GC foundation:

    auto_awg_<state>_<proto>_<port>
    AUTO-AWG: P <proto> <port> <state>

A lazy rule pair may temporarily outlive all state entries for its port in
dev2.

Full age-based garbage collection is deferred to later v0.12 work.

No rules are pre-created for all 65535 ports.

### Implementation order

Implementation is intentionally split into reviewable patches:

1. make apply reconciliation safe for unbounded lazy AUTO-AWG rules;
2. add fail-open migration and rollback cleanup;
3. make HEALTH tuple-aware;
4. convert JUDGE to per-port state;
5. convert FAST and SOFT/DETECT and add lazy rule creation;
6. update status summaries;
7. strict build/render/validate and template-diff gates;
8. isolated RouterOS dry-run acceptance;
9. write-mode testing only after all previous gates pass.
