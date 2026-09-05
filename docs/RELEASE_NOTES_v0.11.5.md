
# Susanin v0.11.5 — stable

v0.11.5 — первый стабильный релиз Susanin для проверенного
ARM64 / RouterOS 7.23.3 reference profile.

Релиз объединяет:

- adaptive routing foundation v0.11.3;
- operational/diagnostic features v0.11.4;
- connection-tracking stability work v0.11.5-dev4.

## Возможности v0.11.3

- credentialless bootstrap;
- automatic LAN discovery;
- tunnel selection;
- routing-table detection/provisioning;
- generated RouterOS source;
- validation before install;
- transactional installation;
- FAST / SOFT / JUDGE / HEALTH;
- separate TCP/UDP learning;
- dynamic learned cache;
- fail-open DIRECT;
- automatic tunnel recovery;
- controller/data-plane separation;
- stage/promote/rollback;
- RouterOS API reply-framing fixes.

## Возможности v0.11.4

- persistent `/data/susanin.conf`;
- configurable log levels;
- NDJSON diagnostic recorder;
- bounded rotation;
- `diag status`;
- `diag start`;
- `diag stop`;
- `diag sample`;
- `diag errors`;
- resource telemetry;
- conntrack telemetry;
- script-job telemetry;
- RouterOS script error collection.

## Изменения v0.11.5

- FAST consolidated filtered conntrack snapshot;
- SOFT/DETECT consolidated snapshot;
- JUDGE consolidated TEST+OK snapshot;
- per-flow transient error isolation;
- race-safe dynamic address-list mutation;
- protected HEALTH cleanup/recovery;
- reduced dynamic conntrack scanning;
- existing detection thresholds preserved;
- existing scheduler intervals preserved;
- existing WATCH/TEST/OK/COOLDOWN state machine preserved.

Data-plane source stable v0.11.5 намеренно не изменяется относительно
known-good v0.11.5-dev4 snapshot.

## Field validation

Reference:

- ARM64 MikroTik;
- RouterOS 7.23.3;
- route-based WireGuard/AmneziaWG egress.

Observed after dev4 promotion:

- validate PASS=4 FAIL=0;
- stage safety gates PASS;
- transactional promote SUCCESS;
- KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0;
- schedulers remained active;
- active test routing observed;
- no new `no such item (4)` during the observed multi-hour dev4 soak window.

## Diagnostics before Issues

Before opening a Bug Issue:

~~~text
diag start
reproduce
diag sample
diag errors
status
apply --dry-run
diag stop
~~~

Full guide:

`docs/LOGGING.md`

## Release assets

- `susanin.tar`
- `install.rsc`
- `uninstall.rsc`
- `uninstall-controller.rsc`
- `SHA256SUMS`

## Scope

Public stable target:

- ARM64;
- RouterOS 7.23.3 reference validation;
- IPv4;
- interface-list `LAN`;
- route-based tunnel;
- WireGuard/AmneziaWG is the most tested path.

Always back up RouterOS before installation or upgrade.
