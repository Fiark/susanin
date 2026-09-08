# Susanin — adaptive VPN routing for MikroTik

[![C11](https://img.shields.io/badge/C-11-blue)](https://en.cppreference.com/w/c/11)
[![RouterOS](https://img.shields.io/badge/RouterOS-tested%207.23.3-293239)](https://mikrotik.com/)
[![Architecture](https://img.shields.io/badge/arch-ARM64-6a5acd)](#requirements)
[![Status](https://img.shields.io/badge/status-stable-brightgreen)](https://github.com/Fiark/susanin/releases/tag/v0.12.0)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Fiark/susanin?label=stable%20release)](https://github.com/Fiark/susanin/releases/latest)

> [!WARNING]
> **Stable v0.12.0**
>
> Susanin changes RouterOS routing and firewall objects.
> Back up your MikroTik before installation, upgrade or uninstall.
>
> Validated public profile: **ARM64 / RouterOS 7.23.3 / IPv4**.

![Susanin](docs/images/hero.svg)

Susanin observes MikroTik RouterOS connection-tracking behavior, detects
destinations that appear unhealthy over the normal DIRECT path, tests them
through an existing VPN routing target, and temporarily remembers the working
route.

Susanin is **not a VPN client**. WireGuard, AmneziaWG or another route-based
VPN must already exist.

In v0.12.0 the adaptive identity is:

~~~text
protocol + destination IPv4 + destination port
~~~

TCP/443 and UDP/443 to the same IP can therefore have independent routing
state.

## Stable v0.12.0

Release:

https://github.com/Fiark/susanin/releases/tag/v0.12.0

Required assets:

~~~text
susanin.tar
install.rsc
~~~

Additional release assets:

~~~text
SHA256SUMS
uninstall.rsc
uninstall-controller.rsc
~~~

Full guide:

[docs/USER_GUIDE_en.md](docs/USER_GUIDE_en.md)

Detailed v0.12.0 release notes:

[docs/RELEASE_NOTES_v0.12.0.md](docs/RELEASE_NOTES_v0.12.0.md)

## What is new in v0.12.0

- routing target **Interface** or **Routing table**;
- native use of existing RouterOS routing tables;
- **VPN Direct** explicit routing policy;
- port-aware adaptive state;
- separate TCP and UDP learning;
- lazy per-port mangle rules;
- `fast`, `middle` and `slow` accuracy profiles;
- migration hardening from legacy IP-only state;
- bounded runtime garbage collection;
- strict IPv4-only operation;
- graceful SIGTERM/SIGINT controller shutdown;
- RouterOS data plane remains active while the controller is stopped.

## Architecture

Susanin has two layers.

### RouterOS data plane

Continuous routing logic runs directly in RouterOS:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Reference installation:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

Dynamic per-port rules are created lazily and are not part of the fixed
structural count.

### Controller

`susanin-controller` performs:

- discovery;
- first-run setup;
- routing-target selection;
- rendering and validation;
- installation;
- status and structural reconciliation;
- configuration;
- VPN Direct policy management;
- diagnostics;
- stage/promote/rollback;
- runtime GC.

User traffic does not traverse the Susanin container.

## Routing targets

v0.12.0 supports two target modes.

### Interface

Select one route-based egress interface.

Susanin may use or provision a dedicated routing table for that interface.

### Routing table

Select an existing RouterOS routing table, for example:

~~~text
r_to_awg
~~~

Susanin delegates forwarding to that table instead of reducing the design to
one gateway/interface.

This preserves RouterOS-native designs such as recursive routing, ECMP,
multi-egress and custom failover.

For a routing-table target Susanin does not automatically own tunnel NAT.

## Requirements

Validated stable reference:

- ARM64 MikroTik with Containers support;
- RouterOS 7.23.3 stable;
- IPv4;
- interface-list named `LAN`;
- existing route-based VPN/tunnel;
- container storage;
- `susanin.tar` and `install.rsc`.

IPv6 adaptive routing is not included in v0.12.0.


## Quick start

### 1. Back up RouterOS

Treat the installation as a routing/firewall change.

### 2. Upload release assets

Upload to MikroTik Files:

~~~text
susanin.tar
install.rsc
~~~

Optional parser check:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Install:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

### 3. Wait for the controller

Check:

~~~routeros
/container print detail where name="susanin-controller"
~~~

During extraction the container may temporarily show:

~~~text
E
~~~

Wait until it becomes RUNNING.

Stable v0.12.0 should report:

~~~text
tag="0.12.0"
arch="arm64"
root-dir=/susanin-controller-v0120
~~~

### 4. Run setup

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin setup" \
    no-sh \
    timeout=300
~~~

Setup offers:

~~~text
1) Interface
2) Routing table
~~~

If you already maintain a dedicated VPN policy-routing table, choose
**Routing table**.

### 5. Verify installation

Version:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin version" \
    no-sh \
    timeout=30
~~~

Expected:

~~~text
Susanin 0.12.0
~~~

Status:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin status" \
    no-sh \
    timeout=60
~~~

Structural reconciliation:

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin apply --dry-run" \
    no-sh \
    timeout=60
~~~

Healthy reference:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## VPN Direct

VPN Direct explicitly forces an IPv4/CIDR/domain through the selected VPN
routing target without waiting for adaptive learning.

Examples:

~~~text
susanin direct add ip 1.1.1.1/32
susanin direct add domain example.com
susanin direct list
susanin direct sync
~~~

Remove entries:

~~~text
susanin direct remove ip 1.1.1.1/32
susanin direct remove domain example.com
~~~

Domain policy depends on DNS visibility in RouterOS.

External DoH, DoT, private DNS or hardcoded IP addresses may bypass domain
population.

TLS SNI inspection is not included in v0.12.0.

## Accuracy profiles

Available profiles:

~~~text
fast
middle
slow
~~~

Change profile:

~~~text
susanin config set accuracy-profile fast
susanin config set accuracy-profile middle
susanin config set accuracy-profile slow
~~~

The final stable field acceptance used `fast`.

## Upgrading

Replacing the controller does not automatically mean the RouterOS data plane
must be replaced.

After controller upgrade run:

~~~text
susanin version
susanin discover
susanin validate
susanin apply --dry-run
~~~

If UPDATE is reported, use:

~~~text
susanin stage
susanin promote --dry-run
susanin promote
susanin snapshot
susanin apply --dry-run
~~~

Full procedure:

[docs/UPGRADE_en.md](docs/UPGRADE_en.md)

## Diagnostics

Before filing a technical issue:

~~~text
susanin diag start

reproduce the problem

susanin diag sample
susanin diag errors
susanin status
susanin apply --dry-run

susanin diag stop
~~~

See:

[docs/LOGGING_en.md](docs/LOGGING_en.md)

Never publish RouterOS backups, `show-sensitive` exports, machine-secret
contents, private keys or passwords.

## Controller shutdown

v0.12.0 handles SIGTERM and SIGINT explicitly.

A normal RouterOS container stop should log:

~~~text
Susanin controller stopping gracefully.
~~~

The already installed RouterOS data plane remains active while the controller
is stopped.

## Uninstall

Full uninstall:

~~~routeros
/import file-name=uninstall.rsc verbose=yes
~~~

This removes Susanin-owned control-plane and adaptive RouterOS objects.

The selected VPN/tunnel itself is preserved.

Controller-only uninstall:

~~~routeros
/import file-name=uninstall-controller.rsc verbose=yes
~~~

This preserves the installed RouterOS adaptive data plane.

## Stable acceptance

Final v0.12.0 acceptance includes:

~~~text
ARM64_RUNTIME=PASS
API_AUTH=PASS
FRESH_BOOTSTRAP=PASS
FRESH_SETUP=PASS
STRUCTURAL_SYNC=PASS

TABLE_NATIVE_TARGET=PASS

VPN_DIRECT_IPV4=PASS
VPN_DIRECT_DOMAIN=PASS
VPN_DIRECT_PRIORITY=PASS

GRACEFUL_SIGTERM=PASS
RESTART_AFTER_SIGTERM=PASS
DATA_PLANE_PRESERVED=PASS

FULL_UNINSTALL=PASS
API_STATE_RESTORED=PASS

INDEPENDENT_AWG_PRESERVED=PASS
INDEPENDENT_ROUTE_PRESERVED=PASS
INDEPENDENT_NAT_PRESERVED=PASS

FRESH_REINSTALL=PASS
GC_IDENTICAL=PASS
~~~

See:

[docs/TESTED.md](docs/TESTED.md)

## Release integrity

The stable v0.12.0 container is a frozen field-tested artifact.

Artifact source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

Exact `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

The stable tag does not trigger an automatic rebuild of this artifact.

See:

[docs/RELEASE_INTEGRITY.md](docs/RELEASE_INTEGRITY.md)

## Documentation

- [Russian README](README.md)
- [English user guide](docs/USER_GUIDE_en.md)
- [Russian full user guide](docs/USER_GUIDE.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Upgrade](docs/UPGRADE_en.md)
- [Logging and diagnostics](docs/LOGGING_en.md)
- [Tested scenarios](docs/TESTED.md)
- [Release integrity](docs/RELEASE_INTEGRITY.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

## License

MIT — see [LICENSE](LICENSE).
