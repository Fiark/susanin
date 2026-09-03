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
