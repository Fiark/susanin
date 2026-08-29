# Contributing

Susanin is an early pilot. Reproducible field reports are especially useful.

Before opening a pull request:

```bash
make clean
make CFLAGS='-O2 -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror'
./tools/secret-scan.sh .
```

For changes to RouterOS bootstrap scripts, also test:

```routeros
/import file-name=install.rsc verbose=yes dry-run
```

Useful issue information:

- RouterOS version;
- ARM64 device family;
- selected tunnel type;
- whether a dedicated routing table already existed;
- sanitized `susanin status` output;
- sanitized `AUTO-AWG:` logs.

Do **not** include backups, `show-sensitive` exports, private keys, VPN configs or passwords.

Large behavior changes should explain how rollback/fail-open is preserved. The project deliberately prefers safe refusal over guessing how to modify a partial RouterOS configuration.
