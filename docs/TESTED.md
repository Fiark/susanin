# Tested scenarios

## v0.11.5 stable reference validation

The stable RouterOS data-plane source is intentionally unchanged
from the known-good v0.11.5-dev4 snapshot.

Validated on the ARM64 / RouterOS 7.23.3 reference router:

- generated source validation `PASS=4 FAIL=0`;
- stage objects validated;
- promotion safety gates passed;
- transactional promotion completed;
- post-promotion reconciliation returned
  `KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0`;
- four RouterOS schedulers continued executing;
- active test/L1 routing was observed;
- transient TEST states continued to appear and expire;
- controller remained independent of the running RouterOS data plane;
- no new `no such item (4)` was observed during the recorded
  multi-hour v0.11.5-dev4 soak window after promotion.

Stable v0.11.5 changes only version/bootstrap/release metadata
and documentation relative to the dev4 data-plane snapshot.

## Public pilot baseline — v0.11.3

Public pilot baseline: **Susanin v0.11.3**.

Reference validation was performed on ARM64 MikroTik running RouterOS 7.23.3 with an already working route-based WireGuard/AmneziaWG tunnel.

Verified:

- static domain/IP routing removed before clean installation;
- `0/16` managed objects detected;
- dry-run reported `READY FOR FRESH INSTALL`;
- generated scripts validated on RouterOS (`PASS=4`);
- transactional install completed with `scripts=4 schedulers=4 mangle=8 safety=3`;
- subsequent status detected the installation;
- subsequent reconciliation returned `KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0`;
- reboot restored container + schedules;
- learned cache restarted and began filling again;
- tunnel unavailable during early boot caused fail-open DIRECT;
- tunnel recovery automatically re-enabled adaptive routing;
- controller upgrade did not require stopping the RouterOS data plane.

## v0.11.4-rc1 preparation validation

The v0.11.4 release candidate is being prepared from the v0.11.4 stability development branch.

Already validated on the RouterOS 7.23.3 ARM64 reference system:

- persistent runtime configuration in `/data/susanin.conf`;
- configurable RouterOS log levels;
- diagnostic recorder start/stop and bounded rotation;
- RouterOS resource / connection-tracking / script-job telemetry;
- `diag errors` collection from RouterOS logs;
- RouterOS API handling that consumes the final `!done` after `!empty`;
- replacement of unsafe bare `:return` exits with `:exit`;
- transient address-list ID guards in the detection path;
- FAST + DETECT operation after the address-list guard without new stale-ID errors during the test window.

A clean uninstall of public v0.11.3 was also tested from a restored RouterOS configuration.

Verified during uninstall:

- Susanin scripts, schedulers, mangle/filter/address-list state removed;
- Susanin controller container and mounts removed;
- Susanin bridge, VETH and controller address removed;
- Susanin API rule and machine user/group removed;
- Susanin configuration and machine secret removed;
- existing AmneziaWG container remained running;
- existing routing table remained present;
- existing default route through the VPN interface remained active;
- ping through the VPN interface continued to work.

The reference router was left as a clean baseline with no Susanin objects and a working external VPN.

## Known RouterOS interaction under investigation

RouterOS 7.23.3 can produce:

~~~text
no such item (4)
~~~

while reading the dynamic connection-tracking table.

This was reproduced directly from a RouterOS terminal with Susanin schedulers disabled and Susanin mangle rules disabled while repeatedly reading `/ip firewall connection`.

Therefore this error is not currently attributed exclusively to Susanin scheduler concurrency.

The issue remains open. The current direction is to reduce or restructure full connection-tracking scans rather than claim the condition is solved.

## v0.11.4-rc1 field validation

The exact GitHub-generated `v0.11.4-rc1` release assets were tested on the ARM64 RouterOS 7.23.3 reference router.

Verified:

- credentialless bootstrap from the published `susanin.tar` and `install.rsc`;
- controller image version `0.11.4-rc1`;
- controller root-dir `/susanin-controller-v0114rc1`;
- automatic detection of `wg-awg-proxy`;
- automatic detection of routing table `r_to_awg`;
- generated RouterOS validation `PASS=4 FAIL=0`;
- fresh install result `scripts=4 schedulers=4 mangle=8 safety=3`;
- existing tunnel masquerade preserved;
- adaptive TCP and UDP learning;
- tunnel DOWN detection and fail-open DIRECT behavior;
- tunnel UP recovery with adaptive mangle rules restored automatically;
- RouterOS reboot with controller and all four schedulers restored;
- full uninstall of Susanin-owned objects;
- external AmneziaWG container and `r_to_awg` route preserved after uninstall;
- successful reinstall using the same GitHub release assets;
- stable CPU and memory usage during soak testing;
- no persistent or stuck Susanin script jobs observed.

The previous bare `:return` / `missing value(s)` RouterOS script failure was not observed after changing script termination to `:exit`.

### Remaining known issue

`no such item (4)` remains reproducible during normal `v0.11.4-rc1` operation.

After the final reinstall, RouterOS diagnostics showed this as the only script-error class observed during the test window.

The data plane continued operating, learning destinations and recovering from tunnel outages despite the intermittent errors.

Investigation remains tracked in GitHub Issue #1.

