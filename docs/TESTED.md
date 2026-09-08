# Tested scenarios

This document records field validation performed for Susanin stable releases.

The current stable reference is **v0.12.0**.

## v0.12.0 stable reference validation

Final v0.12.0 acceptance was performed on a real MikroTik RouterOS system.

Reference platform:

~~~text
RouterOS:      7.23.3 stable
Architecture:  ARM64
Board:         S53UG+5HaxD2HaxD&FG621-EA
LAN:           bridge-LAN / interface-list LAN
LAN IPv4:      192.168.1.1/24
IPv6:          disabled
~~~

Independent VPN infrastructure already existed before Susanin:

~~~text
VPN interface:   wg-awg-proxy
VPN address:     10.8.1.46/32
Routing table:   r_to_awg
Default route:   0.0.0.0/0 via wg-awg-proxy
NAT comment:     AWG selected traffic masquerade
~~~

This infrastructure was intentionally treated as external and not owned by
Susanin.

## Frozen artifact identity

Accepted runtime source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

Exact tested `susanin.tar`:

~~~text
size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f

RouterOS image-id:
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82

binary SHA256:
a9a4b963088682eb27c4ab50eae7bd7800949e0d1f98ec230a769830434b7d51
~~~

Accepted bootstrap assets:

~~~text
192104fc8012958dc511f2f5e7db5ff58be9b7727a287fcfae5e2854407be1c0  install.rsc
6067daacdfb7a047aa4791d2d7d46796dc3a26102c6d9e1e1b1da2f5f40bec98  uninstall.rsc
c5b6751f7d0907cc5db2e904ee32832e92446adfff3e9f9b64d2343ba522d4d3  uninstall-controller.rsc
~~~

The final release bundle contains exactly:

~~~text
susanin.tar
install.rsc
uninstall.rsc
uninstall-controller.rsc
SHA256SUMS
~~~

The field-tested tar was not rebuilt after acceptance.

## ARM64 runtime

Verified:

- container extracted successfully on RouterOS ARM64;
- image reported `linux/arm64`;
- image tag was `0.12.0`;
- runtime command returned:

~~~text
Susanin 0.12.0
~~~

Result:

~~~text
ARM64_RUNTIME=PASS
~~~

## Credentialless bootstrap

Fresh bootstrap was tested from a clean Susanin state.

Verified:

- no RouterOS username/password was requested from the user;
- isolated `bridge-susanin` was created;
- `veth-susanin` was created;
- RouterOS controller address was created;
- restricted `susanin-agent` user/group was provisioned;
- machine secret was generated and verified;
- secret size was 48 bytes;
- secret was mounted as a file;
- persistent `/data` mount was created;
- controller was extracted from the frozen `susanin.tar`;
- controller reached RUNNING;
- temporary bootstrap script/scheduler objects removed themselves.

Final controller identity:

~~~text
name:
susanin-controller

tag:
0.12.0

arch:
arm64

root-dir:
/susanin-controller-v0120

image-id:
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82
~~~

Result:

~~~text
FRESH_BOOTSTRAP=PASS
~~~

## RouterOS API authentication

The controller authenticated using the auto-provisioned local machine account.

Observed:

~~~text
Connecting...
Authenticated.
~~~

Result:

~~~text
API_AUTH=PASS
~~~

## First-run setup

A complete setup was tested after full uninstall, with no Susanin data plane
present.

Selected target:

~~~text
mode:
routing-table

table:
r_to_awg

resolved egress:
wg-awg-proxy

accuracy profile:
fast
~~~

Generated RouterOS source validation:

~~~text
PASS=4
FAIL=0
~~~

Final fresh install result:

~~~text
scripts=4
schedulers=4
mangle=8
safety=3
~~~

For the routing-table target:

~~~text
tunnel NAT=UNMANAGED
~~~

Susanin did not replace the independent AWG NAT rule.

Result:

~~~text
FRESH_SETUP=PASS
~~~

## Production script fingerprints

Final v0.12.0 reference scripts:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Scheduler cadence:

~~~text
auto-awg-health   3s
auto-awg-fast     1s
auto-awg-detect   2s
auto-awg-judge    1s
~~~

## Structural reconciliation

`apply --dry-run` was executed repeatedly on the final runtime, including:

- before graceful shutdown;
- after restart;
- after official frozen-bundle bootstrap;
- after final clean reinstall.

Expected and observed result:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

Result:

~~~text
STRUCTURAL_SYNC=PASS
~~~

## Port-aware runtime state

After fresh installation the RouterOS data plane began building live
port-aware state.

Observed:

- separate TCP/UDP buckets;
- per-port entries;
- lazy per-port AUTO-AWG mangle rules;
- no legacy IP-only adaptive entries;
- no unknown AUTO-AWG rules;
- adaptive migration state reported clean.

A normal status sample included:

~~~text
Summary: scripts=4/4 schedulers=4/4 fixed-mangle=8/8
fixed-duplicates=0
Installation state: detected
Adaptive migration state: clean
~~~

Dynamic lazy-rule count is expected to change with live traffic and therefore
is not part of the fixed-rule reconciliation count.

## Routing-table native target

The final acceptance used an existing table:

~~~text
r_to_awg
~~~

The table contained an independent active default route through:

~~~text
wg-awg-proxy
~~~

Susanin successfully delegated forwarding behavior to the selected table.

The independent route remained active throughout controller stop, uninstall
and reinstall testing.

Result:

~~~text
TABLE_NATIVE_TARGET=PASS
~~~

## VPN Direct — IPv4/CIDR

VPN Direct semantics were tested as:

> explicitly force the selected destination through the selected VPN target,
> bypassing adaptive learning.

Test policy:

~~~text
1.1.1.1/32
~~~

Verified:

- policy persisted in Susanin configuration;
- RouterOS `vpn_direct` address-list was populated;
- `SUSANIN: VPN Direct bypass` was created as a routing-mark rule;
- selected routing mark was `r_to_awg`;
- rule used `passthrough=no`;
- VPN Direct rule was ordered before Susanin safety/adaptive rules;
- live LAN traffic incremented the VPN Direct rule counter;
- egress through `wg-awg-proxy` was observed;
- cleanup removed the temporary policy and RouterOS state.

Result:

~~~text
VPN_DIRECT_IPV4_PERSISTENCE=PASS
VPN_DIRECT_FORCE_TARGET=PASS
VPN_DIRECT_PRIORITY=PASS
VPN_DIRECT_LIVE_LAN_HIT=PASS
VPN_DIRECT_WG_EGRESS=PASS
VPN_DIRECT_IPV4_CLEANUP=PASS
~~~

## VPN Direct — domain

Domain policy was tested with:

~~~text
example.com
~~~

Verified:

- domain policy persisted;
- RouterOS DNS static FWD entry was created;
- `match-subdomain=yes` was used;
- RouterOS DNS populated dynamic `vpn_direct` address-list entries;
- live LAN traffic hit the VPN Direct routing rule;
- traffic to populated addresses was observed leaving through
  `wg-awg-proxy`;
- cleanup removed the policy, DNS static entry, mangle rule and address-list
  state.

Result:

~~~text
VPN_DIRECT_DOMAIN_PERSISTENCE=PASS
VPN_DIRECT_DOMAIN_DNS_FWD=PASS
VPN_DIRECT_DOMAIN_POPULATION=PASS
VPN_DIRECT_DOMAIN_FORCE_TARGET=PASS
VPN_DIRECT_DOMAIN_WG_EGRESS=PASS
VPN_DIRECT_DOMAIN_CLEANUP=PASS
~~~

## VPN Direct domain limitation

The domain test used RouterOS-visible DNS.

The feature depends on RouterOS seeing the DNS lookup.

Encrypted or bypass DNS such as external DoH/DoT/private DNS may prevent
RouterOS from learning the destination addresses.

TLS SNI inspection was not part of v0.12.0 acceptance.

## Graceful controller shutdown

The original daemon behavior could remain sleeping after SIGTERM until
RouterOS eventually killed PID 1.

The final runtime added explicit SIGTERM/SIGINT handling.

Field test:

1. final controller was RUNNING;
2. RouterOS `/container stop` was issued;
3. controller stopped within the acceptance window;
4. no `exited with signal 9 (Killed)` event was produced;
5. RouterOS log contained:

~~~text
Susanin controller stopping gracefully.
~~~

The RouterOS data plane remained installed while the controller was stopped.

Result:

~~~text
GRACEFUL_SIGTERM=PASS
DATA_PLANE_PRESERVED=PASS
~~~

## Restart after graceful shutdown

After the graceful stop the same final controller was started again.

Verified:

- container returned to RUNNING;
- `Susanin 0.12.0` version check passed;
- RouterOS API authentication passed;
- discovery passed;
- structural reconciliation returned IN SYNC;
- all four scripts and all four schedulers remained installed.

Result:

~~~text
RESTART_AFTER_SIGTERM=PASS
~~~

## Full uninstall

The exact frozen `uninstall.rsc` first passed RouterOS parser dry-run:

~~~text
No syntax errors found in the import file
~~~

It was then executed against a fully installed final v0.12.0 system.

Verified removed:

~~~text
susanin-controller
veth-susanin
bridge-susanin
susanin-agent user
susanin-agent group
susanin-secret mount
susanin-data mount
Susanin API firewall rule
4 managed scripts
4 managed schedulers
AUTO-AWG fixed/dynamic mangle
Susanin safety rules
VPN Direct mangle
VPN Direct address-list state
VPN Direct DNS static state
Susanin-owned NAT
Susanin-owned route
susanin.conf
machine secret
~~~

Post-uninstall Susanin object counts were zero.

Result:

~~~text
FULL_UNINSTALL=PASS
~~~

## RouterOS API restoration on uninstall

Before Susanin, the reference RouterOS API service was disabled with no
allowed-address restriction.

After full uninstall the observed state was again:

~~~text
api:
disabled

address:
empty
~~~

Result:

~~~text
API_STATE_RESTORED=PASS
~~~

## Independent VPN infrastructure preservation

Full uninstall was specifically tested against infrastructure that did not
belong to Susanin.

After uninstall:

~~~text
wg-awg-proxy:
present and RUNNING

r_to_awg:
present

0.0.0.0/0 in r_to_awg:
present and ACTIVE

AWG selected traffic masquerade:
present
~~~

Result:

~~~text
INDEPENDENT_AWG_PRESERVED=PASS
INDEPENDENT_ROUTE_PRESERVED=PASS
INDEPENDENT_NAT_PRESERVED=PASS
~~~

## Fresh reinstall after full uninstall

After the complete uninstall, the same frozen release assets remained on the
router.

Pre-install state:

~~~text
susanin-controller=0
managed scripts=0
managed schedulers=0

wg-awg-proxy=1
r_to_awg=1
~~~

The exact same frozen `susanin.tar` was then installed again.

Verified:

- exact tar size `4199936`;
- exact final image-id;
- successful credentialless bootstrap;
- successful first-run setup;
- `PASS=4 FAIL=0` source validation;
- `scripts=4 schedulers=4 mangle=8 safety=3`;
- structural reconciliation IN SYNC;
- VPN Direct policy empty after fresh install;
- persistent configuration present;
- 48-byte machine secret present;
- independent VPN/routing/NAT infrastructure still present.

Result:

~~~text
FRESH_REINSTALL=PASS
~~~

## Final object gate

Final installed reference state:

~~~text
controller running:      1

managed scripts:         4
managed schedulers:      4

fixed mangle rules:      8
safety bypass rules:     3
Susanin NAT:             0

VPN Direct mangle:       0
VPN Direct addresses:    0
VPN Direct DNS:          0

persistent config:       1
machine secret size:     48 bytes

wg-awg-proxy running:    1
r_to_awg present:        1
AWG default active:      1
independent AWG NAT:     1

susanin-agent user:      1
susanin-agent group:     1
Susanin API filter:      1
~~~

## Runtime GC identity

Final v0.12.0 release work did not modify the previously accepted GC source.

Accepted `src/gc.c` SHA256:

~~~text
799050ddd4283a4a9e5a7e96058af697771301b8533f6b3a74220a5d815789cb
~~~

Result:

~~~text
GC_IDENTICAL=PASS
~~~

## v0.12.0 final acceptance summary

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

The final v0.12.0 runtime is frozen.

## Scope of the stable claim

The stable field-validated reference profile is:

- ARM64;
- RouterOS 7.23.3;
- IPv4;
- RouterOS interface-list `LAN`;
- existing route-based VPN/tunnel;
- WireGuard/AmneziaWG reference path.

Other RouterOS versions, architectures and VPN implementations may work, but
they were not part of the final stable v0.12.0 acceptance matrix.

IPv6 adaptive routing is not included in v0.12.0.

TLS SNI inspection is not included in v0.12.0.

## Historical validation

The repository retains detailed development and historical acceptance records:

- `ACCEPTANCE_v0.12.0-dev1.md`;
- `ACCEPTANCE_v0.12.0-dev3.md`;
- `ACCEPTANCE_v0.12.0-dev4.md`;
- `DEV2_PORT_AWARE.md`;
- `DEV3_PROFILES.md`;
- `DEV4_MIGRATION.md`;
- previous release notes.

These files are historical evidence and should not be interpreted as the
current stable user guide.

### v0.11.5

v0.11.5 was the previous stable baseline.

Its acceptance included:

- generated source validation;
- transactional promotion;
- structural reconciliation;
- four running RouterOS schedulers;
- controller/data-plane separation;
- stability work around dynamic RouterOS connection-tracking access.

### v0.11.4 RouterOS interaction

During v0.11.4 testing RouterOS 7.23.3 intermittently produced:

~~~text
no such item (4)
~~~

while reading dynamic connection-tracking entries.

It could also be reproduced directly from a RouterOS terminal with Susanin
workers disabled, so it was not attributed exclusively to Susanin.

This is retained here as historical RouterOS behavior rather than presented
as a new v0.12.0 release blocker.
