# Susanin v0.12 architecture

Base: stable v0.11.5 data plane.

## Routing target

Susanin supports two target modes:

- interface
- routing-table

An interface target may reuse an existing routing table or provision a
dedicated Susanin table.

A routing-table target is used as-is and may internally use any gateway,
interface, multipath path or policy-routing design.

## IPv4 only

Susanin v0.12 is IPv4-only.

Controller RouterOS API sockets use AF_INET.
An active IPv6 Internet default route is treated as an unmanaged bypass.
Setup must either block on it or explicitly disable IPv6 in strict mode.

## VPN Direct

VPN Direct is one user-visible persistent policy list.

Supported entries:

- IPv4 address or CIDR
- domain

VPN Direct has absolute priority over adaptive routing.

Domain entries use RouterOS DNS FWD/address-list resolution.
TCP TLS flows also use tls-host/SNI verification where observable.

## Adaptive identity

Adaptive state is keyed by:

    protocol + destination IPv4 + destination port

Example:

    tcp / 203.0.113.10 / 443 -> AWG
    tcp / 203.0.113.10 / 80  -> DIRECT

The same IP on different ports therefore learns independently.

## Accuracy profiles

FAST:
Current v0.11.5 behavior is the baseline.
A strong DIRECT failure can immediately enter TEST.
One healthy AWG test may confirm the tuple.

MIDDLE:
Require repeated independent DIRECT evidence.
Require repeated healthy AWG evidence before confirmation.

SLOW:
Maximum-confidence mode.
Use repeated DIRECT failures, SNI-assisted verification when available,
AWG success, a DIRECT re-check and a second AWG success before confirmation.

No mode claims mathematical 100 percent certainty because remote service
state, CDN changes, QUIC, ECH and unavailable/fragmented TLS SNI can prevent
perfect observation.

## SNI verification

For a suspicious TCP/TLS tuple Susanin may:

1. find DNS-cache names currently mapped to the destination IPv4;
2. create temporary tls-host observer rules scoped to the source client,
   destination IPv4 and destination port;
3. force a clean reconnect;
4. identify the matching SNI by rule counters;
5. remove all temporary observer rules.

SNI is an additional confidence signal, not a mandatory dependency.

## Port state

Port-aware WATCH/TEST/OK/COOLDOWN state uses native RouterOS timeout
semantics.

Implementation should use per-protocol/per-port dynamic state lists and
lazily managed mangle rules, with garbage collection of unused port rules.

## Development sequence

0.12.0-dev1:
configuration schema, routing target abstraction, IPv4-only enforcement,
VPN Direct persistence and CLI.

0.12.0-dev2:
port-scoped adaptive state and dynamic data-plane rules.

0.12.0-dev3:
FAST/MIDDLE/SLOW evidence state machines.

0.12.0-dev4:
SNI-assisted verification, migration, garbage collection, fail-open and
long-running acceptance.
