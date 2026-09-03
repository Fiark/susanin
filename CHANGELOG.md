# Changelog

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
