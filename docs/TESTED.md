# Tested scenarios

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
