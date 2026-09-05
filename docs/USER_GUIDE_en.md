# Susanin v0.11.5 — User Guide

Susanin v0.11.5 is the first stable release for the validated ARM64 / RouterOS 7.23.3 reference profile.

Susanin is not a VPN client. A route-based VPN/tunnel must already exist.

## What Susanin does

Susanin:

- discovers LAN networks from interface-list `LAN`;
- selects a route-based tunnel;
- detects or provisions a routing table;
- renders RouterOS scripts for the actual LAN topology;
- validates generated source before installation;
- installs FAST / SOFT / JUDGE / HEALTH workers;
- learns TCP and UDP destinations independently;
- fails open to DIRECT if the tunnel is unavailable;
- restores adaptive routing when the tunnel returns.

## v0.11.3 foundation

v0.11.3 introduced:

- credentialless bootstrap;
- internal `susanin-agent`;
- mounted machine secret;
- automatic tunnel selection;
- routing-table discovery/provisioning;
- transactional installation;
- FAST / SOFT / JUDGE / HEALTH;
- protocol-separated learned cache;
- fail-open DIRECT;
- controller/data-plane separation;
- stage/promote/rollback.

## v0.11.4 operations and diagnostics

v0.11.4 introduced:

- `/data/susanin.conf`;
- configurable decision logging;
- `quiet/error/info/debug/trace`;
- diagnostic NDJSON recorder;
- bounded rotation;
- `diag status/start/stop`;
- `diag sample`;
- `diag errors`;
- CPU/RAM/conntrack/script-job telemetry.

## v0.11.5 stability work

v0.11.5 uses the known-good dev4 data plane:

- one filtered FAST conntrack snapshot;
- one consolidated SOFT/DETECT snapshot;
- one TEST+OK JUDGE snapshot;
- per-flow transient error isolation;
- guarded dynamic address-list mutations;
- hardened HEALTH cleanup/recovery;
- existing thresholds and scheduler cadence preserved.

# Installation

Upload:

~~~text
susanin.tar
install.rsc
~~~

Parser check:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Install:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Wait until:

~~~routeros
/container print where name="susanin-controller"
~~~

shows `R`.

Run setup:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin setup" \
  no-sh timeout=300
~~~

# Health check

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin status" \
  no-sh timeout=60
~~~

Structural reconciliation:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin apply --dry-run" \
  no-sh timeout=60
~~~

Healthy state:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

# Data plane

## FAST

Detects fast failure signals such as:

- unanswered TCP SYN;
- short failed TCP CLOSE;
- unanswered QUIC/UDP 443.

## SOFT / DETECT

Detects slower failure patterns:

- TCP stall;
- late TCP stall with debounce;
- unanswered UDP;
- QUIC late stall.

## JUDGE

Tests the destination through the selected tunnel.

Successful tunnel retry:

~~~text
TEST -> OK
~~~

Failed tunnel retry:

~~~text
TEST -> COOLDOWN -> DIRECT
~~~

TCP and UDP are learned independently.

## HEALTH

After two tunnel misses, adaptive mangle is disabled and traffic fails open to DIRECT.

After tunnel recovery, adaptive routing is automatically enabled again.

# Runtime cache

~~~text
auto_awg_watch_tcp
auto_awg_test_tcp
auto_awg_ok_tcp
auto_awg_cooldown_tcp

auto_awg_watch_udp
auto_awg_test_udp
auto_awg_ok_udp
auto_awg_cooldown_udp
~~~

These are temporary state/cache lists, not a permanent site database.

# Controller commands

Common:

~~~text
susanin setup
susanin status
susanin apply --dry-run
susanin version
~~~

Discovery/validation:

~~~text
susanin discover
susanin plan
susanin render
susanin validate
susanin snapshot
~~~

Install/update:

~~~text
susanin install --dry-run
susanin install
susanin stage
susanin stage-clean
susanin promote --dry-run
susanin promote
susanin rollback
~~~

Configuration:

~~~text
susanin config show
susanin config set log-level quiet|error|info|debug|trace
susanin config set diagnostics on|off
susanin config set diagnostic-max-size-mb 1..100
susanin config set diagnostic-max-files 1..10
~~~

Diagnostics:

~~~text
susanin diag status
susanin diag start
susanin diag stop
susanin diag sample
susanin diag errors
~~~

# Upgrading

Do not assume that replacing the container also upgrades existing RouterOS script source.

Use the full procedure in:

[UPGRADE_en.md](UPGRADE_en.md)

The essential flow is:

~~~text
version
validate
apply --dry-run
    |
    +-- UPDATE=0 -> done
    |
    +-- UPDATE>0
          |
          stage
          promote --dry-run
          Safety gates: PASS
          promote
          snapshot
          apply --dry-run
~~~

# Diagnostics before Bug Issues

Before filing a runtime bug:

~~~text
diag start
reproduce
diag sample
diag errors
status
apply --dry-run
diag stop
~~~

See:

[LOGGING_en.md](LOGGING_en.md)

Never publish RouterOS backups, `show-sensitive` exports, private keys, passwords or `susanin-secrets/routeros_password`.
