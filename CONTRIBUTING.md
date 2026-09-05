
# Contributing

Susanin welcomes reproducible field reports and focused code changes.

## Before opening a Bug Issue

Collect diagnostics first:

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

`docs/LOGGING.md`

Include:

- Susanin version;
- RouterOS version;
- ARM64 device family;
- tunnel type;
- topology;
- sanitized status;
- sanitized reconciliation;
- telemetry sample;
- script-error summary;
- relevant AUTO-AWG logs;
- sanitized diagnostic NDJSON.

Never include:

- RouterOS backup;
- `show-sensitive` export;
- private keys;
- API credentials;
- VPN credentials;
- passwords;
- `susanin-secrets/routeros_password`.

## Before opening a Pull Request

~~~bash
make clean
make CFLAGS='-O2 -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror'
./tools/secret-scan.sh .
~~~

For bootstrap changes:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

For RouterOS data-plane changes:

~~~text
susanin validate
susanin stage
susanin promote --dry-run
~~~

Large behavioral changes should explain:

- fail-open behavior;
- rollback behavior;
- RouterOS object impact;
- conntrack scan impact;
- real RouterOS diagnostic evidence.

Susanin prefers safe refusal over guessing how to modify
a partial or ambiguous RouterOS configuration.
