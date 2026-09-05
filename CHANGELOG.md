# Changelog

## v0.12.0-dev1 — routing control-plane foundation

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
