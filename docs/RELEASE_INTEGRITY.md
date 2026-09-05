# Release integrity

## v0.11.5 stable

Git commit:

~~~text
eaa64074a1b2f815f0de17365b12c9710f52357b
~~~

Published `susanin.tar` SHA256:

~~~text
5ec4e0625144b46c7cd4ee6cad8a991bdb1945e731a348473ba2a049af98ed98
~~~

The release also contains `SHA256SUMS` covering:

- `susanin.tar`;
- `install.rsc`;
- `uninstall.rsc`;
- `uninstall-controller.rsc`.

Always verify downloaded artifacts before deployment.

## Release policy after v0.11.5

The release workflow is designed to:

1. validate tag/source/bootstrap version consistency;
2. build ARM64;
3. execute the ARM64 binary under QEMU;
4. generate checksums;
5. create a DRAFT release;
6. upload all assets while still draft;
7. publish only after assets are complete;
8. refuse to overwrite an already published release.

Repository-level GitHub Release Immutability should also be enabled by the repository administrator for future releases.
