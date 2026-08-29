# Architecture

Susanin splits the system into two layers.

## RouterOS data plane

Runs continuously inside RouterOS:

- `auto-awg-fast` — fast failure signals;
- `auto-awg-detect` — soft/stall signals;
- `auto-awg-judge` — checks whether retry through the selected tunnel helped;
- `auto-awg-health` — tunnel health, fail-open and recovery.

The scripts manipulate temporary address lists and connection/routing marks. User traffic does not traverse the Susanin container.

## C11 control plane

The container performs:

- RouterOS discovery;
- first-run setup;
- script rendering;
- syntax/read-back validation;
- transactional fresh installation;
- status and structural reconciliation;
- staged upgrades and rollback.

This design means that a controller restart or upgrade does not stop the current adaptive routing data plane.

## Protocol-separated learning

TCP and UDP use separate `ok/test/watch/cooldown` lists. This prevents a successful QUIC retry from automatically classifying TCP to the same IP as healthy through the tunnel (and vice versa).

## Fail-open

Health failures disable the managed routing mangle rules after a debounce. Clients then use normal DIRECT routing. When the tunnel recovers, rules are re-enabled and the recovery sweep removes stale direct connections to already-confirmed destinations.
