# Susanin v0.11.4-rc1 — release candidate draft

## Status

Release candidate preparation branch.

This document describes the changes accumulated after the v0.11.3 pilot baseline.

## Highlights

- persistent runtime configuration in `/data/susanin.conf`;
- configurable RouterOS log levels;
- diagnostic recorder with rotation;
- RouterOS telemetry collection from the controller;
- RouterOS script error diagnostics via `diag errors`;
- safer RouterOS script termination handling;
- protection against transient RouterOS address-list object races.

## Validation performed

Reference test platform:

- MikroTik ARM64;
- RouterOS 7.23.3;
- route-based AmneziaWG/WireGuard egress.

Verified scenarios:

- clean uninstall of the v0.11.3 controller while preserving external VPN data plane;
- clean RouterOS state after uninstall;
- fresh ARM64 container build;
- diagnostics start/stop/sample/error reporting;
- stage/validate/promote workflow.

## Known limitations

- ARM64 remains the public target;
- RouterOS 7.23.3 is the reference validation platform;
- `no such item (4)` from dynamic `/ip firewall connection` access remains tracked as a RouterOS interaction issue and is not considered fully solved in this release candidate.

## Field validation of the published RC

The exact GitHub-generated `v0.11.4-rc1` artifacts were validated on the ARM64 RouterOS 7.23.3 reference router.

Passed:

- fresh credentialless installation;
- generated script validation (`PASS=4 FAIL=0`);
- adaptive TCP and UDP learning;
- tunnel DOWN fail-open DIRECT;
- tunnel UP recovery;
- reboot persistence;
- full uninstall while preserving the external VPN;
- reinstall using the same published assets;
- short soak with stable CPU, memory and scheduler operation;
- no persistent Susanin script jobs observed.

The previous bare `:return` / `missing value(s)` failure was not observed.

Known issue:

- intermittent RouterOS `no such item (4)` errors remain reproducible while the data plane is active;
- the data plane continued operating despite these errors;
- Issue #1 remains open for the connection-tracking investigation.

