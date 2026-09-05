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
