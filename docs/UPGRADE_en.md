# Upgrading Susanin

This is the supported Susanin upgrade procedure starting with v0.11.5.

Susanin has two independent parts:

- the container is the control plane;
- RouterOS `auto-awg-*` scripts are the data plane.

Replacing the container does **not** prove that existing RouterOS script source was upgraded.

## Procedure

Back up RouterOS first.

Upload the new:

~~~text
susanin.tar
install.rsc
SHA256SUMS
~~~

Check the bootstrap parser:

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

Upgrade the controller:

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

Verify:

~~~routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin version" no-sh timeout=30
/container/shell susanin-controller cmd="/usr/local/bin/susanin validate" no-sh timeout=120
/container/shell susanin-controller cmd="/usr/local/bin/susanin apply --dry-run" no-sh timeout=60
~~~

If `UPDATE=0`, the data plane is already in sync.

If updates are required:

~~~routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin stage" no-sh timeout=300
/container/shell susanin-controller cmd="/usr/local/bin/susanin promote --dry-run" no-sh timeout=120
~~~

Continue only with:

~~~text
Safety gates: PASS
~~~

Then:

~~~routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin promote" no-sh timeout=300
/container/shell susanin-controller cmd="/usr/local/bin/susanin snapshot" no-sh timeout=60
/container/shell susanin-controller cmd="/usr/local/bin/susanin apply --dry-run" no-sh timeout=60
~~~

Healthy final state:

~~~text
KEEP=16
CREATE=0
UPDATE=0
BLOCKERS=0
Result: IN SYNC structurally.
~~~

For upgrades from v0.11.3/v0.11.4 to v0.11.5, do not stop after replacing the controller. Validate and promote the RouterOS data plane as described above.

See [LOGGING_en.md](LOGGING_en.md) if the upgrade exposes a problem.
