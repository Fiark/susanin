# Changelog

## v0.12.0 — stable

Stable release for the validated ARM64 / RouterOS 7.23.3 reference profile.

v0.12.0 moves Susanin from the previous IP-oriented adaptive model to a
port-aware routing architecture and adds explicit routing-target control.

### Highlights

- two routing target modes: **Interface** and **Routing table**;
- native use of existing RouterOS routing tables;
- routing-table targets may preserve recursive routing, ECMP and
  multi-egress designs;
- port-aware adaptive identity:
  `protocol + destination IPv4 + destination port`;
- lazy per-port AUTO-AWG mangle rules;
- separate TCP and UDP learning;
- accuracy profiles: `fast`, `middle`, `slow`;
- persistent VPN Direct policy;
- VPN Direct IPv4/CIDR support;
- VPN Direct domain support through RouterOS DNS;
- migration hardening from legacy v0.11.x IP-only state;
- bounded runtime garbage collection;
- strict IPv4-only operation;
- controller/data-plane separation retained;
- graceful SIGTERM/SIGINT controller shutdown.

### VPN Direct

VPN Direct means:

> force the selected IP, CIDR or domain through the configured VPN routing
> target without adaptive learning.

The final implementation uses a RouterOS `mark-routing` rule with
`passthrough=no`.

VPN Direct is placed before Susanin safety/adaptive rules so the explicit
operator policy has priority.

Domain policies use RouterOS DNS static FWD entries with
`match-subdomain=yes` and `address-list=vpn_direct`.

Domain routing therefore depends on DNS visibility in RouterOS. Clients using
external DoH, DoT, private DNS or hardcoded IP addresses may bypass domain
population.

TLS SNI inspection is not included in v0.12.0.

### Routing-table target

Susanin can use an existing routing table directly instead of reducing it to
one selected output interface.

The accepted reference configuration used:

~~~text
routing table:
r_to_awg

resolved egress:
wg-awg-proxy
~~~

For routing-table targets Susanin does not create or own tunnel NAT
automatically.

### Port-aware adaptive state

Adaptive state now distinguishes destinations by:

~~~text
protocol + destination IPv4 + destination port
~~~

For example, TCP/443 and UDP/443 to the same IP are independent states.

Dynamic per-port mangle rules are created lazily and are not part of the
fixed-rule reconciliation count.

### Data plane

Final reference production scripts:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Reference installed state:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

Structural reconciliation:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

### Migration and runtime cleanup

v0.12.0 cleans incompatible legacy adaptive runtime state before starting the
new port-aware data plane.

Cleanup covers:

- legacy IP-only lists;
- old port-state objects;
- stale lazy per-port rules;
- adaptive connection marks.

Runtime GC bounds Susanin-owned dynamic state.

The accepted GC implementation remained unchanged during final release
hardening.

### Graceful controller shutdown

The controller now handles SIGTERM and SIGINT explicitly.

A normal RouterOS container stop exits cleanly instead of waiting for
RouterOS to force PID 1 down with SIGKILL.

The RouterOS adaptive data plane continues operating while the controller is
stopped.

### Uninstall behavior

The final `uninstall.rsc` was tested against a complete installation.

It removes Susanin-owned:

- controller objects;
- RouterOS scripts and schedulers;
- adaptive mangle state;
- safety rules;
- VPN Direct state;
- API machine account/rule;
- persistent Susanin configuration and machine secret.

It preserves independently managed VPN/routing infrastructure.

The reference acceptance confirmed preservation of:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

RouterOS API state was also restored to its pre-Susanin state after full
uninstall.

### Final field acceptance

Validated on:

- RouterOS 7.23.3 stable;
- ARM64;
- real MikroTik hardware;
- existing WireGuard/AmneziaWG routing path.

Final acceptance includes:

- ARM64 runtime;
- credentialless bootstrap;
- API authentication;
- fresh setup;
- table-native routing target;
- generated-source validation `PASS=4 FAIL=0`;
- structural reconciliation;
- VPN Direct IPv4/CIDR forced routing;
- VPN Direct domain forced routing;
- VPN Direct priority;
- graceful shutdown;
- controller restart;
- data-plane preservation;
- complete uninstall;
- independent VPN/routing/NAT preservation;
- fresh reinstall from the exact same frozen assets.

### Frozen release artifact

Accepted runtime source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

Exact field-tested `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f

RouterOS image-id:
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82
~~~

The stable release uses the frozen field-tested artifact and does not rebuild
`susanin.tar` from the later documentation/release commit.

### Stable scope

Validated stable scope:

- ARM64;
- RouterOS 7.23.3;
- IPv4;
- interface-list `LAN`;
- existing route-based VPN/tunnel;
- WireGuard/AmneziaWG reference path.

Not included in stable v0.12.0:

- IPv6 adaptive routing;
- TLS SNI inspection;
- SNI-based verification.

Detailed validation:

- `docs/TESTED.md`;
- `docs/RELEASE_NOTES_v0.12.0.md`;
- `docs/RELEASE_INTEGRITY.md`.


## v0.12.0-dev1 — routing control-plane foundation

### v0.12.0-dev1 acceptance

RouterOS 7.23.3 ARM64 acceptance completed successfully.

- table-native routing target accepted against an existing routing table;
- VPN Direct IPv4/CIDR accepted;
- VPN Direct domain and subdomain DNS FWD behavior accepted;
- external DNS requests through RouterOS populate dynamic `vpn_direct`
  IPv4 entries;
- RouterOS internal `:resolve` does not populate that address-list;
- repeated synchronization and full removal are clean and idempotent;
- VPN Direct bypass precedes adaptive AUTO-AWG rules;
- production v0.11.5 FAST/SOFT/JUDGE remained unchanged;
- strict IPv4-only operation remains mandatory before production use.


### v0.12.0-dev1 table-native target follow-up

- HEALTH now probes the selected RouterOS routing table directly instead of
  requiring an IPv4 address on one selected tunnel interface;
- routing-table targets may use recursive routing, ECMP, or multiple egress
  interfaces without Susanin binding the data plane to one interface;
- interface targets retain legacy automatic NAT management;
- routing-table targets deliberately leave NAT ownership to the selected
  external routing design;
- first-run setup now lets the operator choose Interface or Routing table;
- setup exposes usable non-LAN interfaces rather than only tunnel-looking names;
- strict IPv4-only setup sets RouterOS `disable-ipv6=yes` and never reboots
  automatically;
- status and discovery report the selected target abstraction;
- FAST, SOFT and JUDGE behavior remain unchanged from the v0.11.5 baseline.


- keep the published v0.11.5 adaptive data-plane behavior as the baseline;
- add persistent routing target metadata: interface or routing-table;
- add `target show`, `target list`, and explicit target selection commands;
- keep a resolved interface/table compatibility pair for the proven v0.11.5
  health and routing scripts during the first v0.12 development stage;
- add persistent `VPN Direct` policy with IPv4/CIDR and domain entries;
- synchronize VPN Direct IP entries into RouterOS address-list `vpn_direct`;
- synchronize VPN Direct domains through RouterOS DNS FWD rules with
  `match-subdomain=yes` and `address-list=vpn_direct`;
- install a VPN Direct mangle bypass before adaptive AUTO-AWG rules;
- add FAST/MIDDLE/SLOW accuracy profile configuration; dev1 still executes
  the current FAST behavior until the evidence state machine is implemented;
- force controller RouterOS API sockets to IPv4 (`AF_INET`).

## v0.11.5 — stable

First stable release for the validated ARM64 / RouterOS 7.23.3 reference profile.

v0.11.5 combines the adaptive-routing foundation introduced in v0.11.3,
the operational/diagnostic features introduced in v0.11.4,
and the connection-tracking stability work from v0.11.5-dev4.

Highlights:

- consolidated FAST conntrack snapshot;
- consolidated SOFT/DETECT conntrack snapshot;
- consolidated JUDGE TEST+OK snapshot;
- per-flow transient error isolation;
- race-safe address-list mutations;
- protected HEALTH cleanup and recovery;
- reduced dynamic conntrack scanning;
- existing FAST/SOFT/JUDGE thresholds preserved;
- existing scheduler cadence preserved;
- existing WATCH/TEST/OK/COOLDOWN state machine preserved;
- stable release packaging;
- expanded user documentation;
- expanded logging/diagnostics documentation;
- diagnostic capture required for technical Bug Issues.

Operational features inherited from v0.11.4:

- persistent `/data/susanin.conf`;
- configurable RouterOS logging levels;
- diagnostic NDJSON recorder;
- bounded rotation;
- `diag status/start/stop`;
- `diag sample`;
- `diag errors`;
- RouterOS resource telemetry;
- conntrack telemetry;
- script-job telemetry.

Public stable scope:

- ARM64;
- RouterOS 7.23.3 reference platform;
- IPv4;
- interface-list `LAN`;
- route-based tunnel;
- WireGuard/AmneziaWG is the most tested egress.

## v0.11.4-rc1 — release candidate

Release candidate preparation after the v0.11.3 public pilot baseline.

Highlights:

- persistent runtime configuration in `/data/susanin.conf`;
- configurable RouterOS log levels;
- diagnostic recorder with bounded rotation;
- RouterOS resource and script-job telemetry;
- RouterOS log diagnostics through `diag errors`;
- safer RouterOS script termination using `:exit`;
- transient RouterOS address-list object race protection;
- release workflow version consistency validation.

### Validation performed

- ARM64 MikroTik reference platform;
- RouterOS 7.23.3;
- existing route-based WireGuard/AmneziaWG egress preserved;
- clean uninstall of v0.11.3 from restored configuration;
- Susanin-owned objects removed without affecting external VPN routing;
- diagnostics, telemetry and staged update workflow tested.

### Known limitations

- ARM64 remains the public target;
- RouterOS 7.23.3 is the reference validation platform;
- IPv4 only;
- `no such item (4)` from dynamic `/ip firewall connection` access remains under investigation;
- final GitHub-built RC artifact installation is still pending.

## v0.11.3 — pilot public baseline

First public pilot candidate proven on a real ARM64 MikroTik / RouterOS 7.23.3.

Highlights:

- credentialless bootstrap with internal `susanin-agent`;
- random secret written to a mounted file, not env/argv;
- exact secret read-back verification before agent password rotation;
- self-cleaning elevated bootstrap worker;
- automatic VPN/tunnel selection UI;
- routing-table auto-detection or dedicated `susanin` table provisioning;
- dynamic LAN rendering from interface-list `LAN`;
- generated RouterOS script validation before fresh-install commit;
- transactional fresh install from `0/16` managed objects to `16/16`;
- FAST / SOFT / JUDGE / HEALTH data plane;
- protocol-separated TCP/UDP learned cache;
- fail-open DIRECT behavior when the tunnel is unavailable;
- controller/data-plane separation;
- stage / promote / rollback update path;
- RouterOS API `!empty` framing bug fixed by consuming the final `!done`.

### Real-world validation performed

- clean install from a restored RouterOS configuration after removing old static domain/IP routing;
- controller upgrade while data plane remained active;
- reboot after clean install;
- VPN-not-ready-at-boot fail-open and later automatic recovery;
- structural `KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0` reconciliation.

### Known limitations

- ARM64 only in the public pilot;
- tested baseline is RouterOS 7.23.3;
- IPv4 only;
- setup currently expects interface-list `LAN`;
- WireGuard/AmneziaWG path is the main tested egress;
- heuristic thresholds are experimental and can produce false positives/negatives.
