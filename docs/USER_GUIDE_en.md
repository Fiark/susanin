# Susanin v0.12.0 — User Guide

Susanin is a control plane for adaptive VPN routing in MikroTik RouterOS.

It observes connection-tracking behavior, tests suspicious destinations
through an existing VPN routing target, and temporarily remembers the working
path.

Susanin is not a VPN client. WireGuard, AmneziaWG or another route-based VPN
must already exist.

Back up RouterOS before installation, upgrade or uninstall.

## Validated stable profile

Stable v0.12.0 was field-tested on:

- ARM64;
- RouterOS 7.23.3 stable;
- IPv4;
- interface-list `LAN`;
- an existing route-based VPN/tunnel;
- WireGuard/AmneziaWG reference routing.

Other platforms may work but are outside the validated stable profile.

## What changed in v0.12.0

Major changes from v0.11.5:

- routing target `Interface` or `Routing table`;
- native routing-table targets;
- VPN Direct;
- port-aware adaptive state;
- lazy per-port mangle rules;
- `fast`, `middle`, `slow` accuracy profiles;
- migration hardening;
- bounded runtime GC;
- strict IPv4-only operation;
- graceful SIGTERM/SIGINT shutdown.

The adaptive identity is:

~~~text
protocol + destination IPv4 + destination port
~~~

For example:

~~~text
tcp + 203.0.113.10 + 443
udp + 203.0.113.10 + 443
tcp + 203.0.113.10 + 8443
~~~

are three independent states.

# 1. Architecture

Susanin separates the system into a control plane and a RouterOS data plane.

## 1.1. Controller

`susanin-controller` performs:

- discovery;
- setup;
- target selection;
- rendering;
- validation;
- installation;
- structural reconciliation;
- configuration;
- VPN Direct policy management;
- diagnostics;
- stage/promote/rollback;
- runtime GC.

User traffic does not traverse the controller.

## 1.2. RouterOS data plane

Four managed scripts run continuously:

~~~text
auto-awg-health
auto-awg-fast
auto-awg-detect
auto-awg-judge
~~~

Reference installed structure:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

Dynamic per-port rules are runtime state and may change while traffic flows.

## 1.3. Controller independence

Stopping the controller does not remove the installed RouterOS data plane.

This allows:

- controller restart without losing current routing;
- independent controller upgrades;
- fail-open behavior without container forwarding dependency.

# 2. Routing targets

v0.12.0 supports two target modes.

## 2.1. Interface

Select one route-based egress interface.

Example:

~~~text
wg-vpn
~~~

CLI:

~~~text
susanin target set interface wg-vpn
~~~

Susanin may use or provision a routing table for this interface.

## 2.2. Routing table

Select an existing RouterOS routing table.

Example:

~~~text
r_to_awg
~~~

CLI:

~~~text
susanin target set routing-table r_to_awg
~~~

The shorter alias is also accepted:

~~~text
susanin target set table r_to_awg
~~~

Susanin delegates forwarding to the selected table.

This preserves RouterOS-native designs such as:

- recursive routing;
- ECMP;
- multiple egress interfaces;
- custom failover;
- custom route distances.

For a routing-table target Susanin does not automatically own tunnel NAT.

# 3. VPN Direct

VPN Direct is an explicit persistent policy.

Its v0.12.0 meaning is:

> force this IPv4, CIDR or domain through the selected VPN routing target
> without waiting for adaptive learning.

It is not a DIRECT bypass.

Examples:

~~~text
susanin direct add ip 1.1.1.1/32
susanin direct add ip 203.0.113.0/24
susanin direct add domain example.com
susanin direct list
susanin direct sync
~~~

Remove entries:

~~~text
susanin direct remove ip 1.1.1.1/32
susanin direct remove domain example.com
~~~

VPN Direct has priority over ordinary adaptive routing rules.

## 3.1. Domain policy

Domain entries use RouterOS DNS static FWD rules with:

~~~text
match-subdomain=yes
address-list=vpn_direct
~~~

RouterOS-visible DNS responses populate IPv4 entries in `vpn_direct`.

External DoH, DoT, private DNS or hardcoded IP addresses may bypass this
mechanism.

TLS SNI inspection is not included in v0.12.0.


# 4. Accuracy profiles

Available profiles:

~~~text
fast
middle
slow
~~~

Show current configuration:

~~~text
susanin config show
~~~

Set a profile:

~~~text
susanin config set accuracy-profile fast
susanin config set accuracy-profile middle
susanin config set accuracy-profile slow
~~~

The final v0.12.0 field acceptance used:

~~~text
fast
~~~

After changing a profile, validate the desired data plane:

~~~text
susanin validate
susanin apply --dry-run
~~~

If UPDATE is reported, use the staged upgrade procedure.

# 5. Installation

## 5.1. Back up RouterOS

Susanin changes RouterOS firewall, routing, scripts, schedulers and controller
infrastructure.

Create a backup and make sure you have a recovery path before installation.

## 5.2. Download the stable release

Required:

~~~text
susanin.tar
install.rsc
~~~

Recommended additional files:

~~~text
SHA256SUMS
uninstall.rsc
uninstall-controller.rsc
~~~

## 5.3. Verify SHA256

On Linux:

~~~bash
sha256sum -c SHA256SUMS
~~~

Expected:

~~~text
susanin.tar: OK
install.rsc: OK
uninstall.rsc: OK
uninstall-controller.rsc: OK
~~~

## 5.4. Upload to MikroTik

Upload through WinBox/WebFig Files:

~~~text
susanin.tar
install.rsc
~~~

Keep these exact filenames.

## 5.5. Parser dry-run

Recommended:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

The parser check should complete without syntax errors.

## 5.6. Bootstrap

Run:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

The bootstrap automatically:

- creates the isolated controller network;
- provisions the restricted machine identity;
- generates and verifies the machine secret;
- creates mounts;
- extracts `susanin.tar`;
- starts `susanin-controller`;
- removes temporary bootstrap helpers.

No user RouterOS API password is requested.

## 5.7. Wait for RUNNING

Check:

~~~routeros
/container print detail where name="susanin-controller"
~~~

During extraction RouterOS may show:

~~~text
E
~~~

Wait until the container is RUNNING.

Stable v0.12.0 should report:

~~~text
tag="0.12.0"
arch="arm64"
root-dir=/susanin-controller-v0120
~~~

## 5.8. Run first-time setup

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

Choose `Interface` for a simple single route-based egress.

Choose `Routing table` when an existing RouterOS policy-routing table should
remain authoritative.

Reference acceptance used:

~~~text
routing table:
r_to_awg

resolved egress:
wg-awg-proxy
~~~

## 5.9. Expected fresh install

Generated source validation:

~~~text
PASS=4 FAIL=0
~~~

Reference install:

~~~text
Fresh install result: SUCCESS
scripts=4 schedulers=4 mangle=8 safety=3
~~~

For a routing-table target:

~~~text
tunnel NAT=UNMANAGED
~~~

# 6. Post-install verification

## 6.1. Version

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

## 6.2. Discovery

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin discover" \
    no-sh \
    timeout=60
~~~

Verify:

- `Authenticated.`;
- LAN;
- target mode;
- target name;
- routing table;
- expected egress.

## 6.3. Status

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin status" \
    no-sh \
    timeout=60
~~~

Healthy reference:

~~~text
scripts=4/4
schedulers=4/4
fixed-mangle=8/8
fixed-duplicates=0

Installation state: detected
Adaptive migration state: clean
~~~

Dynamic lazy per-port rule counts may be non-zero.

## 6.4. Structural reconciliation

~~~routeros
/container/shell susanin-controller \
    cmd="/usr/local/bin/susanin apply --dry-run" \
    no-sh \
    timeout=60
~~~

Expected:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~


# 7. Port-aware runtime state

v0.12.0 identifies adaptive state by:

~~~text
protocol + destination IPv4 + destination port
~~~

For one destination IP, these can all be different states:

~~~text
tcp/443 -> OK
udp/443 -> TEST
tcp/8443 -> COOLDOWN
~~~

This matters for CDNs, shared IP addresses, QUIC and servers exposing multiple
services.

## 7.1. TCP and UDP separation

TCP and UDP are learned independently.

A successful UDP/443 retry does not automatically classify TCP/443 to the same
IP as successful through the VPN target.

## 7.2. Lazy per-port rules

Susanin does not pre-create rules for every possible port.

Per-port AUTO-AWG rules are created lazily when runtime state requires them.

A healthy status may therefore contain:

~~~text
fixed-mangle=8/8
lazy-mangle=N
~~~

where `N` changes with traffic.

Dynamic lazy rules are not structural drift.

`apply --dry-run` reconciles the fixed data plane.

# 8. Runtime GC

Adaptive runtime state is temporary.

Runtime GC bounds Susanin-owned dynamic objects such as:

- port-aware state;
- stale lazy rules;
- temporary adaptive entries;
- related runtime objects.

Manual GC:

~~~text
susanin gc
~~~

The controller daemon also performs periodic GC.

GC must not remove independent VPN interfaces, routing tables, user routes or
user NAT rules.

# 9. CLI reference

All commands below run inside `susanin-controller`.

## 9.1. Core

~~~text
susanin version
susanin discover
susanin plan
susanin status
susanin apply --dry-run
susanin snapshot
susanin render
susanin validate
~~~

## 9.2. Installation

~~~text
susanin setup
susanin install --dry-run
susanin install
~~~

## 9.3. Routing target

~~~text
susanin target show
susanin target list
susanin target set interface <name>
susanin target set routing-table <name>
~~~

## 9.4. VPN Direct

~~~text
susanin direct list
susanin direct add ip <IPv4[/prefix]>
susanin direct add domain <domain>
susanin direct remove ip <IPv4[/prefix]>
susanin direct remove domain <domain>
susanin direct sync
~~~

## 9.5. Configuration

~~~text
susanin config show
susanin config set accuracy-profile fast|middle|slow
susanin config set log-level quiet|error|info|debug|trace
susanin config set diagnostics on|off
~~~

## 9.6. Safe upgrade

~~~text
susanin stage
susanin stage-clean
susanin promote --dry-run
susanin promote
susanin rollback
~~~

## 9.7. Diagnostics

~~~text
susanin diag status
susanin diag start
susanin diag stop
susanin diag sample
susanin diag errors
~~~

## 9.8. Runtime

~~~text
susanin gc
susanin daemon
~~~

# 10. Renderer and validation

## 10.1. render

~~~text
susanin render
~~~

Generates desired RouterOS source without replacing production scripts.

## 10.2. validate

~~~text
susanin validate
~~~

Generated source is validated through temporary RouterOS objects.

Reference result:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

## 10.3. snapshot

~~~text
susanin snapshot
~~~

Shows production script fingerprints.

Stable v0.12.0 reference:

~~~text
auto-awg-health   6148   cafdf828c49d2946
auto-awg-fast     26098  0c9672d93a6a4e85
auto-awg-detect   41519  0ee9c8e6708bc6e8
auto-awg-judge    16840  72733543f4561160
~~~

# 11. Safe data-plane upgrade

Replacing the controller and changing the RouterOS data plane are separate
operations.

After a controller upgrade first run:

~~~text
susanin version
susanin discover
susanin validate
susanin apply --dry-run
~~~

If the result is:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

the production data plane already matches.

If UPDATE is reported, use staged promotion.

## 11.1. stage

~~~text
susanin stage
~~~

Creates inert stage scripts without replacing production.

## 11.2. promote --dry-run

~~~text
susanin promote --dry-run
~~~

Checks promotion safety gates.

Do not run a real promotion while blockers remain.

## 11.3. promote

~~~text
susanin promote
~~~

Promotion performs a transactional source replacement with rollback backup,
scheduler pause, verification and scheduler restoration.

After promotion:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

## 11.4. rollback

~~~text
susanin rollback
~~~

Then verify:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

## 11.5. stage-clean

~~~text
susanin stage-clean
~~~

Removes inert stage objects.

Full upgrade procedure:

[UPGRADE_en.md](UPGRADE_en.md)


# 12. Diagnostics

See:

[LOGGING_en.md](LOGGING_en.md)

Common commands:

~~~text
susanin diag status
susanin diag start
susanin diag sample
susanin diag errors
susanin diag stop
~~~

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

Do not publish secrets with diagnostics.

# 13. Security

Susanin uses an automatically provisioned local RouterOS machine identity:

~~~text
susanin-agent
~~~

The bootstrap creates an isolated controller network and generates a random
48-character machine secret.

The secret is stored through a mounted file and is not intended to be passed
through container environment variables or command-line arguments.

Never publish:

- RouterOS backups;
- `show-sensitive` exports;
- machine-secret contents;
- WireGuard private keys;
- AmneziaWG private keys;
- API passwords;
- VPN credentials.

Avoid broad `/file print detail` commands against Susanin files because
RouterOS may include file contents in the output.

See:

[../SECURITY.md](../SECURITY.md)

# 14. Controller shutdown and restart

Stable v0.12.0 handles SIGTERM and SIGINT.

A normal RouterOS stop:

~~~routeros
/container stop [find where name="susanin-controller"]
~~~

should log:

~~~text
Susanin controller stopping gracefully.
~~~

The RouterOS data plane remains installed while the controller is stopped.

After restart verify:

~~~text
susanin version
susanin discover
susanin status
susanin apply --dry-run
~~~

# 15. Full uninstall

Recommended parser check:

~~~routeros
/import file-name=uninstall.rsc verbose=yes dry-run
~~~

Full uninstall:

~~~routeros
/import file-name=uninstall.rsc verbose=yes
~~~

The script removes Susanin-owned:

- controller infrastructure;
- machine user/group;
- mounts;
- API firewall rule;
- managed scripts;
- schedulers;
- adaptive mangle state;
- safety rules;
- VPN Direct state;
- Susanin configuration;
- machine secret.

It does not remove the selected VPN/tunnel itself.

Final acceptance verified preservation of independent:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

# 16. Controller-only uninstall

~~~routeros
/import file-name=uninstall-controller.rsc verbose=yes
~~~

This removes the control plane while preserving the installed RouterOS
adaptive data plane.

# 17. Troubleshooting

## 17.1. Container shows E

`E` means RouterOS is still extracting the image.

Wait until the container becomes RUNNING.

## 17.2. RouterOS API authentication fails

Check:

~~~routeros
/ip service print detail where name="api"
~~~

and the Susanin firewall rule.

Do not expose RouterOS API broadly to the LAN or Internet.

## 17.3. Routing target is missing

Run:

~~~text
susanin discover
susanin target list
~~~

Verify that the expected interface or routing table exists and is enabled.

## 17.4. VPN Direct domain does not populate

Check:

~~~text
susanin direct list
susanin direct sync
~~~

Then verify that RouterOS can see client DNS requests.

External DoH/DoT/private DNS may bypass the domain population mechanism.

## 17.5. apply --dry-run reports UPDATE

Do not replace production script source manually.

Use:

~~~text
susanin validate
susanin stage
susanin promote --dry-run
~~~

and only promote after the safety gates pass.

## 17.6. Traffic falls back to DIRECT after VPN failure

This may be expected fail-open behavior.

Check:

~~~text
susanin status
susanin target show
~~~

After target recovery, adaptive routing should restore automatically.

# 18. Stable v0.12.0 acceptance

Final field acceptance:

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

[TESTED.md](TESTED.md)

# 19. Release integrity

Stable v0.12.0 uses a frozen field-tested artifact.

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

The stable tag does not trigger an automatic rebuild of the accepted artifact.

See:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

# 20. Stable limitations

v0.12.0 does not include:

- IPv6 adaptive routing;
- TLS SNI inspection;
- SNI-based verification.

VPN Direct domain routing depends on RouterOS-visible DNS.

The validated public architecture is ARM64.

Reference RouterOS is 7.23.3.

# 21. Documentation

- [English README](../README_en.md)
- [Russian README](../README.md)
- [Russian User Guide](USER_GUIDE.md)
- [Architecture](ARCHITECTURE.md)
- [Upgrade](UPGRADE_en.md)
- [Logging and diagnostics](LOGGING_en.md)
- [Tested scenarios](TESTED.md)
- [Release integrity](RELEASE_INTEGRITY.md)
- [Security](../SECURITY.md)
- [Changelog](../CHANGELOG.md)
