# Susanin v0.12 — historical development roadmap

> **Status: completed and archived.**
>
> This file records the development direction that led to stable v0.12.0.
> It is not the authoritative description of the released feature set.
>
> For stable behavior see:
>
> - [../README.md](../README.md)
> - [ARCHITECTURE.md](ARCHITECTURE.md)
> - [USER_GUIDE.md](USER_GUIDE.md)
> - [TESTED.md](TESTED.md)
> - [RELEASE_NOTES_v0.12.0.md](RELEASE_NOTES_v0.12.0.md)

## Starting point

Development of v0.12 started from the stable v0.11.5 data plane.

The main goals were:

- routing-target abstraction;
- IPv4-only enforcement;
- VPN Direct;
- port-aware adaptive state;
- accuracy profiles;
- migration hardening;
- runtime garbage collection;
- fail-open behavior;
- long-running acceptance.

## Routing target

The v0.12 design introduced two user-facing target modes:

~~~text
interface
routing-table
~~~

Stable v0.12.0 supports both.

A routing-table target delegates forwarding policy to the selected RouterOS
table.

The final accepted RouterOS 7.23.3 configuration also had a concrete resolved
IPv4 egress for HEALTH probing.

## IPv4-only scope

v0.12.0 is intentionally IPv4-only.

IPv6 adaptive routing is outside the stable v0.12.0 scope.

## VPN Direct

The original v0.12 work introduced a persistent policy for:

- IPv4 addresses;
- IPv4 CIDRs;
- domains.

The **final stable semantics** are:

> force matching traffic through the selected VPN routing target without
> waiting for adaptive learning.

VPN Direct has priority over ordinary adaptive routing.

Domain policy uses RouterOS-visible DNS to populate `vpn_direct`.

External DoH, DoT, private DNS or hardcoded IP addresses may bypass domain
population.

## Adaptive identity

The final v0.12.0 adaptive identity is:

~~~text
protocol + destination IPv4 + destination port
~~~

For example:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

are independent states.

## Port-aware runtime

v0.12.0 introduced:

- port-scoped adaptive state;
- lazy per-port mangle rules;
- cleanup of stale runtime state;
- bounded runtime GC.

Dynamic lazy rules are runtime state and are not treated as fixed structural
drift.

## Accuracy profiles

The v0.12 development line introduced:

~~~text
fast
middle
slow
~~~

These profiles control the tradeoff between reaction speed and the amount of
evidence required by the adaptive decision process.

## Migration

Moving from the v0.11.x IP-oriented state model to the v0.12.0 port-aware
model required explicit migration handling.

Transactional promotion clears incompatible adaptive runtime state before
managed schedulers resume.

## Fail-open and lifecycle hardening

The final v0.12.0 work also included:

- fail-open handling;
- structural reconciliation;
- staged promotion;
- rollback;
- runtime GC;
- graceful SIGTERM/SIGINT controller shutdown;
- preservation of the RouterOS data plane when the controller stops.

## SNI research — not shipped

During development, SNI-assisted verification was considered as an additional
confidence signal.

That experimental direction included ideas such as temporary `tls-host`
observer rules and matching TLS SNI against DNS-derived names.

**SNI verification was not included in stable v0.12.0.**

Stable v0.12.0 does not perform:

- TLS SNI inspection;
- SNI-assisted adaptive verification;
- SNI-based VPN Direct matching.

This roadmap must not be used as evidence that those features exist.

## Development sequence

Historical development phases:

~~~text
dev1
routing target abstraction
IPv4-only work
VPN Direct configuration and CLI

dev2
port-aware adaptive state
lazy dynamic data-plane rules

dev3
fast / middle / slow accuracy profiles

dev4
migration
garbage collection
fail-open
acceptance hardening
~~~

SNI-assisted verification was explored during the dev4 planning period but
was excluded from the final stable scope.

## Final stable result

Stable v0.12.0 ultimately shipped with:

- Interface and Routing table targets;
- VPN Direct with force-through-VPN semantics;
- IPv4/CIDR/domain policy;
- port-aware adaptive identity;
- lazy per-port rules;
- fast/middle/slow profiles;
- migration hardening;
- bounded runtime GC;
- fail-open behavior;
- graceful controller shutdown;
- staged promotion and rollback;
- strict IPv4-only scope.

Final field acceptance is documented in:

[TESTED.md](TESTED.md)

Exact frozen release identity is documented in:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

## Superseded status

This roadmap is retained only as development history.

If this file conflicts with stable v0.12.0 documentation, the stable
documentation and field-acceptance records take precedence.
