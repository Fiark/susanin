# Susanin v0.11.5 — Logging and Diagnostics

Susanin has two different diagnostic mechanisms:

1. RouterOS decision log (`AUTO-AWG:`);
2. internal Susanin NDJSON recorder.

They complement each other.

# Decision logging

Show current configuration:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config show" \
  no-sh timeout=30
~~~

Levels:

| Level | Behavior |
|---|---|
| `quiet` | almost no decision logging |
| `error` | important health/fail-open events |
| `info` | error + recovery + CONFIRMED |
| `debug` | info + FAST/SOFT detection |
| `trace` | accepted; v0.11.5 verbosity is effectively similar to debug |

Default:

~~~text
info
~~~

Set debug:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin config set log-level debug" \
  no-sh timeout=30
~~~

The selected log level is rendered into RouterOS script source.

Therefore `apply --dry-run` may show UPDATE after changing it.
Use stage/promote to apply the generated source safely.

# RouterOS decision log

Live:

~~~routeros
/log print follow-only where message~"AUTO-AWG:"
~~~

Typical messages:

~~~text
FAST TCP-SYN
FAST TCP-CLOSE
FAST QUIC
SOFT TCP-STALL
SOFT TCP-LATE-STALL
SOFT UDP
CONFIRMED ... via tcp
CONFIRMED ... via udp
tunnel DOWN ... DIRECT
tunnel UP ...
~~~

# Internal recorder

Status:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag status" \
  no-sh timeout=30
~~~

Start:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag start" \
  no-sh timeout=30
~~~

Container path:

~~~text
/data/diagnostics/susanin-debug.ndjson
~~~

Stop:

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag stop" \
  no-sh timeout=30
~~~

The recorder does **not** continuously copy RouterOS `/log`.

It records controller events and results collected by diagnostic commands.

# diag sample

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag sample" \
  no-sh timeout=60
~~~

Collects:

- CPU load;
- free/total memory;
- uptime;
- RouterOS version;
- board/architecture;
- conntrack total/max;
- script jobs;
- managed Susanin workers.

# diag errors

~~~routeros
/container/shell susanin-controller \
  cmd="/usr/local/bin/susanin diag errors" \
  no-sh timeout=60
~~~

Reports:

- RouterOS records scanned;
- script error count;
- `no such item` count;
- latest `no such item`;
- recent script errors.

# Rotation

Defaults:

~~~text
diagnostic_max_size_mb=10
diagnostic_max_files=3
~~~

Supported ranges:

~~~text
1..100 MB
1..10 files
~~~

# Required runtime Bug Issue capture

Before opening a runtime bug:

~~~text
1. diag start
2. reproduce the problem
3. diag sample
4. diag errors
5. status
6. apply --dry-run
7. capture relevant AUTO-AWG log
8. diag stop
~~~

# Export diagnostic files

RouterOS Files path:

~~~text
susanin-data/diagnostics/
~~~

Check:

~~~routeros
/file print where name~"susanin-data/diagnostics"
~~~

Download only the relevant diagnostic files using WinBox Files, SCP, or another normal RouterOS file-transfer method.

Do not export `susanin-secrets`.

# Do not publish

Never attach:

~~~text
RouterOS .backup
/export show-sensitive
susanin-secrets/routeros_password
WireGuard private keys
AmneziaWG private keys
VPN credentials
API credentials
passwords
~~~

Review IP addresses, ports, interface names and topology metadata before posting logs publicly.
