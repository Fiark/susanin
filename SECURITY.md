# Security policy

Susanin v0.12.0 is the current stable release for the validated
ARM64 / RouterOS 7.23.3 reference profile.

Susanin changes RouterOS firewall/routing objects. Treat every installation,
upgrade and uninstall as a network change with a tested backup and rollback
path.

## Supported security scope

The validated stable scope is:

- ARM64;
- RouterOS 7.23.3;
- IPv4;
- interface-list `LAN`;
- existing route-based VPN/tunnel.

Security assumptions may differ on other RouterOS releases, architectures or
network designs.

## Credential model

Susanin does not ask the user for a RouterOS API username or password.

`bootstrap/install.rsc` creates an isolated controller network:

~~~text
RouterOS bridge-susanin: 172.31.254.1/30
container veth-susanin:  172.31.254.2/30
~~~

A temporary bootstrap worker:

1. generates a random 48-character machine secret;
2. writes it to `susanin-secrets/routeros_password`;
3. reads the value back;
4. verifies the written secret before changing the machine account;
5. creates or updates the restricted `susanin-agent`;
6. mounts the secret into the controller;
7. starts `susanin-controller`;
8. removes temporary elevated bootstrap helpers.

The secret is not passed through container environment variables or
command-line arguments.

## RouterOS API

Susanin v0.12.0 uses RouterOS API on TCP/8728 inside the isolated controller
network.

The bootstrap restricts access to the controller source:

~~~text
172.31.254.2/32
~~~

and creates:

~~~text
SUSANIN: allow controller API
~~~

The API connection itself is not transport encrypted.

Do not expose TCP/8728 to the Internet or broadly to the LAN for Susanin.

If RouterOS API already contains other trusted source addresses, the bootstrap
preserves them.

Full uninstall only disables and clears API access automatically when Susanin
was its only configured source.

## Machine account

The long-lived RouterOS identity is:

~~~text
susanin-agent
~~~

It receives only the policies required by the controller:

~~~text
read,write,test,api
~~~

Temporary bootstrap permissions such as `policy` and `password` belong to the
bootstrap worker, not the long-lived agent.

After successful bootstrap there should be no remaining
`susanin-bootstrap-*` scripts or schedulers.

## Sensitive files

Never publish:

- RouterOS `.backup` files;
- `/export show-sensitive`;
- `susanin-secrets/routeros_password` contents;
- WireGuard private or preshared keys;
- AmneziaWG private keys;
- VPN credentials;
- RouterOS API passwords;
- unrelated container environment dumps containing secrets.

Do not use broad `/file print detail` commands against Susanin files during
diagnostics. RouterOS may include file contents in the output.

For the machine secret, inspect metadata only.

For example, checking the file size is sufficient. Stable bootstrap uses a
48-byte secret.

If the machine secret is accidentally shown in a terminal transcript,
screenshot, issue or log, treat that value as compromised and rotate it.

## Data handled by adaptive routing

The RouterOS data plane works with connection metadata such as:

- source/destination IPv4;
- protocol;
- destination port;
- TCP state;
- packet/byte counters;
- reply state;
- connection marks.

The adaptive routing algorithm does not require packet payload inspection.

v0.12.0 does not perform TLS SNI inspection.

## VPN Direct and DNS

VPN Direct domain policies use RouterOS-visible DNS to populate
`vpn_direct` IPv4 entries.

This means RouterOS may observe domain names used for VPN Direct policy.

Clients using external DoH, DoT or private DNS may bypass this mechanism.

Operators should consider whether RouterOS DNS/log metadata is acceptable for
their environment.

## Diagnostics and logs

Diagnostics may contain network metadata such as:

- destination IPv4;
- destination port;
- RouterOS version;
- board model;
- routing target;
- script errors;
- resource counters.

Sanitize diagnostics before publishing them.

Do not include secrets, private keys, full sensitive exports or RouterOS
backup files in public issues.

## Controller and data-plane boundary

User traffic does not pass through `susanin-controller`.

The container is a control plane.

The continuous adaptive data plane runs inside RouterOS.

Stopping the controller does not automatically remove the installed data
plane.

v0.12.0 handles SIGTERM/SIGINT for graceful controller shutdown.

## Ownership boundary

Susanin uninstall is intended to remove Susanin-owned objects without deleting
independently managed VPN infrastructure.

Final v0.12.0 acceptance verified preservation of an external:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

Review custom installations carefully if Susanin-created and user-created
objects share unusual names or ownership patterns.

## Release integrity

Stable v0.12.0 uses a frozen field-tested artifact.

Runtime source:

~~~text
d53517dfd6daccb7073517661138d79c573cac46
~~~

The stable tag does not trigger a rebuild of the accepted `susanin.tar`.

Verify downloaded assets using `SHA256SUMS`.

See `docs/RELEASE_INTEGRITY.md`.

## Reporting a vulnerability

Do not publish secrets or exploit details that could endanger users in a
public issue.

For non-sensitive reports include:

- RouterOS version;
- architecture/model family without serial number;
- Susanin version;
- minimal reproduction;
- sanitized logs;
- expected behavior;
- actual behavior.

For sensitive vulnerability reports, use GitHub private security reporting
when it is available for the repository.
