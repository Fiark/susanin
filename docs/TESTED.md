# Tested scenarios

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

## Not yet verified for v0.11.4-rc1

The final GitHub-built `v0.11.4-rc1` artifacts have not yet been installed on the clean reference router.

Still required:

- fresh install from the clean baseline;
- first-run setup;
- reboot;
- tunnel DOWN fail-open;
- tunnel UP recovery;
- diagnostics and `diag errors`;
- full uninstall;
- reinstall;
- extended soak with all four RouterOS workers enabled.
