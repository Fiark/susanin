# Susanin v0.12.0 — Architecture

Susanin separates routing control from packet forwarding.

The system consists of:

~~~text
Susanin controller
        |
        | RouterOS API
        v
RouterOS data plane
        |
        v
DIRECT or selected VPN routing target
~~~

User traffic never traverses the Susanin container.

## 1. Design goals

The v0.12.0 architecture is built around several principles:

- keep the continuous data plane inside RouterOS;
- keep the container out of the forwarding path;
- preserve routing while the controller is stopped;
- use RouterOS connection tracking as the primary evidence source;
- keep adaptive state temporary;
- distinguish traffic by protocol, destination IPv4 and destination port;
- allow explicit user policy to override adaptive learning;
- fail open to DIRECT when the selected VPN target is unhealthy;
- preserve independently managed VPN/routing infrastructure;
- keep runtime state bounded.

## 2. Control plane

The C11 container is the Susanin control plane.

It performs:

- RouterOS discovery;
- first-run setup;
- routing-target selection;
- configuration persistence;
- RouterOS script rendering;
- generated-source validation;
- installation;
- structural reconciliation;
- snapshots;
- staged upgrades;
- promotion and rollback;
- VPN Direct policy management;
- diagnostics;
- runtime GC;
- graceful daemon lifecycle.

The controller communicates with RouterOS through a restricted local API
identity provisioned by the bootstrap.

The controller is not a proxy.

## 3. RouterOS data plane

The continuous adaptive data plane runs directly in RouterOS.

Managed scripts:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Reference scheduler cadence:

~~~text
auto-awg-health    3s
auto-awg-fast      1s
auto-awg-detect    2s
auto-awg-judge     1s
~~~

Reference fixed mangle structure:

~~~text
8 adaptive rules
3 safety bypass rules
~~~

Dynamic per-port rules are separate runtime objects.

## 4. Why the data plane stays in RouterOS

Keeping continuous processing in RouterOS avoids an API round-trip for every
FAST/DETECT/JUDGE cycle.

It also means:

- current adaptive routing survives controller restart;
- controller upgrade does not automatically stop schedulers;
- packet forwarding does not depend on container availability;
- RouterOS firewall and connection tracking remain authoritative;
- control-plane failures are isolated from normal forwarding.

The controller can therefore be stopped while the already installed data
plane continues running.

## 5. Routing target abstraction

v0.12.0 introduces a routing-target abstraction.

Two modes exist:

~~~text
interface
routing-table
~~~

The adaptive data plane marks selected traffic for the configured target.

## 5.1. Interface target

An interface target represents one route-based egress interface.

Example:

~~~text
wg-vpn
~~~

Susanin may use an existing suitable routing table or provision a dedicated
FIB table and default route through that interface.

This mode is intended for relatively simple single-egress designs.

## 5.2. Routing-table target

A routing-table target delegates forwarding to an existing RouterOS table.

Example:

~~~text
r_to_awg
~~~

Susanin does not reduce the table to one interface or gateway.

The table may therefore contain:

- recursive routes;
- ECMP;
- multiple egress interfaces;
- failover;
- custom route distances;
- other RouterOS-native routing logic.

The routing table remains the forwarding authority.

For this mode Susanin deliberately does not automatically own tunnel NAT.

## 6. Adaptive identity

The fundamental v0.12.0 adaptive identity is:

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

This prevents one service on a shared IP from classifying unrelated traffic
to the same address.

TCP and UDP are also independent.

## 7. Adaptive state machine

A simplified path is:

~~~text
DIRECT observation
      |
      v
WATCH / suspicious evidence
      |
      v
TEST through VPN target
      |
      +---- success ----> OK
      |
      +---- failure ----> COOLDOWN / DIRECT
~~~

FAST and DETECT produce evidence.

JUDGE evaluates whether routing through the selected target improved the
connection.

HEALTH controls fail-open and recovery.


## 8. VPN Direct

VPN Direct is an explicit operator policy that bypasses adaptive learning.

Its meaning in v0.12.0 is:

~~~text
force this IPv4/CIDR/domain through the selected VPN routing target
~~~

It does not mean DIRECT bypass.

For IPv4/CIDR entries Susanin synchronizes RouterOS address-list:

~~~text
vpn_direct
~~~

The final routing rule uses:

~~~text
chain=prerouting
action=mark-routing
new-routing-mark=<selected routing table>
passthrough=no
dst-address-type=!local
dst-address-list=vpn_direct
in-interface-list=LAN
~~~

The VPN Direct rule is placed before Susanin safety and adaptive prerouting
rules.

This gives explicit operator policy priority over adaptive decisions.

## 8.1. VPN Direct domains

Domain entries use RouterOS DNS static FWD rules with:

~~~text
match-subdomain=yes
address-list=vpn_direct
~~~

RouterOS-visible DNS responses populate dynamic `vpn_direct` IPv4 entries.

Therefore domain policy depends on DNS visibility.

External DoH, DoT, private DNS or hardcoded IP addresses may bypass this
population mechanism.

TLS SNI inspection is not part of v0.12.0.

## 9. Lazy per-port rules

The port-aware model does not pre-create routing rules for every possible
destination port.

Instead, Susanin creates per-port AUTO-AWG rules lazily when corresponding
runtime state appears.

The system therefore has two classes of routing objects:

~~~text
fixed structural rules
dynamic lazy per-port rules
~~~

Fixed rules are reconciled by `apply --dry-run`.

Dynamic lazy rules are runtime state.

For example, a healthy installation may report:

~~~text
fixed-mangle=8/8
lazy-mangle=4
~~~

while structural reconciliation still reports:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 10. Accuracy profiles

v0.12.0 supports:

~~~text
fast
middle
slow
~~~

The selected profile is stored in persistent Susanin configuration.

Profiles change how aggressively evidence is accumulated and how quickly
adaptive state progresses.

The reference stable acceptance used:

~~~text
fast
~~~

A profile change may alter generated RouterOS source.

The safe workflow is therefore:

~~~text
config change
    |
    v
validate
    |
    v
apply --dry-run
    |
    +--> IN SYNC --> no production change required
    |
    +--> UPDATE --> stage / promote
~~~

## 11. Runtime state and garbage collection

Port-aware adaptive routing creates temporary state.

Examples include:

- WATCH entries;
- TEST entries;
- OK entries;
- COOLDOWN entries;
- lazy per-port rules;
- adaptive connection marks.

This state is intentionally temporary.

Runtime GC keeps Susanin-owned dynamic state bounded.

GC must not delete independently managed:

- VPN interfaces;
- routing tables;
- user routes;
- user NAT;
- unrelated firewall objects.

The final v0.12.0 release preserves the separately accepted GC
implementation unchanged during final release hardening.

## 12. Migration boundary

v0.11.x primarily used IP-oriented adaptive state.

v0.12.0 introduces port-aware identity.

Mixing incompatible old state with the new model could produce incorrect
routing decisions.

Therefore setup/install cleans incompatible runtime residue before starting
the v0.12.0 data plane.

Cleanup includes:

- legacy IP-only adaptive lists;
- stale port-state objects;
- stale lazy rules;
- adaptive connection marks.

A normal final status reports:

~~~text
legacy IP-only entries=0
Adaptive migration state: clean
~~~

## 13. Fail-open behavior

Susanin is designed so that VPN failure does not automatically become a
general connectivity failure.

HEALTH monitors the selected routing target.

Simplified behavior:

~~~text
target healthy
    |
    v
adaptive routing enabled

target unhealthy
    |
    v
managed adaptive routing disabled
    |
    v
normal DIRECT routing

target recovered
    |
    v
adaptive routing restored
~~~

This is a fail-open design.

## 14. Controller lifecycle

The controller daemon is not required for every forwarded packet.

A normal controller stop sends SIGTERM.

v0.12.0 handles SIGTERM and SIGINT explicitly.

Expected shutdown log:

~~~text
Susanin controller stopping gracefully.
~~~

Stopping the controller does not remove:

- RouterOS scripts;
- schedulers;
- fixed mangle rules;
- already installed adaptive data plane.

After restart the controller reloads persistent configuration and can inspect
the existing RouterOS state again.

Reference post-restart reconciliation:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 15. Ownership boundaries

Susanin distinguishes its own objects from external infrastructure.

Examples of Susanin-owned objects:

~~~text
susanin-controller
veth-susanin
bridge-susanin
susanin-agent
auto-awg-*
SUSANIN: safety bypass*
SUSANIN: VPN Direct bypass
vpn_direct
~~~

External objects may include:

~~~text
VPN interface
routing table
default route
NAT
other firewall policy
~~~

When an existing routing table is selected, Susanin must not assume ownership
of that table's internal routing design.

This ownership boundary is especially important during uninstall.

The final field acceptance verified that full Susanin uninstall preserved:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

## 16. Bootstrap security boundary

The bootstrap creates an isolated controller network:

~~~text
RouterOS: 172.31.254.1
Susanin:  172.31.254.2
~~~

A restricted machine identity is provisioned:

~~~text
susanin-agent
~~~

The machine secret is generated automatically and stored through a mounted
secret file.

It is not intended to be supplied manually by the user through environment
variables or command arguments.

The controller API firewall rule is scoped to the isolated controller source.

## 17. IPv4-only boundary

Stable v0.12.0 is intentionally IPv4-only.

Adaptive identity contains destination IPv4, not IPv6.

VPN Direct supports IPv4/CIDR and domain-derived IPv4 state.

IPv6 adaptive routing is outside the stable v0.12.0 scope.

## 18. Structural reconciliation

Susanin separates fixed configuration from transient runtime state.

`apply --dry-run` compares desired fixed structure against production.

The final reference result is:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

The sixteen fixed reconciled objects are:

~~~text
4 managed scripts
4 managed schedulers
8 fixed adaptive mangle rules
~~~

Safety rules and dynamic runtime state are validated through their respective
installation/status paths rather than counted as those sixteen fixed objects.

## 19. Reference production identity

Final v0.12.0 production script fingerprints:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Final fresh-install reference:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

## 20. Release boundary

The final runtime artifact is frozen.

Artifact source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

Field-tested `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

Later documentation/release commits are allowed only while runtime/build
inputs remain identical to the artifact source.

Stable release publication does not rebuild the accepted container artifact.

See:

- `RELEASE_INTEGRITY.md`;
- `TESTED.md`;
- `RELEASE_NOTES_v0.12.0.md`.
