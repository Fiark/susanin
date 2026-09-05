# Security policy

Susanin v0.11.5 is the first **stable** release for the validated ARM64 / RouterOS 7.23.3 reference profile. Susanin changes RouterOS firewall/routing objects, so every installation or upgrade must still be treated as a network change with a tested backup and rollback path.

## Supported security scope

The current public stable target is ARM64 RouterOS. The reference validation platform is RouterOS 7.23.3. Security assumptions may differ on other RouterOS releases or architectures.

## Credential model

Susanin does not ask the user for a RouterOS API username or password.

`bootstrap/install.rsc` creates an isolated control network:

```text
RouterOS bridge-susanin: 172.31.254.1/30
container veth-susanin: 172.31.254.2/30
```

A temporary admin-owned bootstrap worker:

1. generates a random 48-character machine secret;
2. writes it to `susanin-secrets/routeros_password`;
3. reads it back as a string;
4. verifies size, length and exact byte-equivalent text before changing the machine account;
5. creates/updates `susanin-agent` with `read,write,test,api`;
6. mounts the secret at `/run/secrets/routeros_password`;
7. starts the controller;
8. schedules a one-shot cleanup that removes the elevated bootstrap worker and its scheduler.

The password is not passed via container environment variables or command-line arguments.

## RouterOS API

The current stable release uses plain RouterOS API on TCP/8728 **inside the isolated /30 control network**. The bootstrap adds `172.31.254.2/32` to the API allowed-address set and inserts a narrow input firewall allow rule.

This is not equivalent to transport encryption. API-SSL is a future hardening target.

If the API service already had other trusted source addresses, the installer preserves them and appends the Susanin controller address. Administrators should review `/ip service print detail where name="api"` after installation.

## Temporary elevated permissions

The bootstrap worker needs temporary permissions including `policy` and `password` so it can create the restricted machine account. Those permissions are not granted to the long-lived `susanin-agent`. The helper is removed after successful controller start.

Check that no bootstrap helpers remain:

```routeros
/system scheduler print where name~"susanin-bootstrap-"
/system script print where name~"susanin-bootstrap-"
```

## Data handled by the adaptive logic

The RouterOS scripts inspect connection-tracking metadata such as:

- source/destination IP;
- protocol and destination port;
- TCP state;
- packet/byte counters;
- reply presence/rates;
- RouterOS connection marks.

Susanin does not need packet payload, DNS names or TLS SNI for routing decisions.

RouterOS logs can contain destination IPs and ports. Decide whether that metadata is acceptable for your environment before enabling persistent/remote logging.

## Sensitive files

Never publish or attach to issues:

- RouterOS `.backup` files;
- `/export show-sensitive` output;
- WireGuard private/preshared keys;
- VPN configuration files containing credentials;
- `susanin-secrets/routeros_password` contents;
- container env dumps from unrelated VPN containers.

The repository includes `tools/secret-scan.sh` as a basic release hygiene check. It is not a substitute for a real secret scanner or manual review.

## Reporting a vulnerability

For now, open a GitHub issue **only if the report does not require publishing a secret or exploit details that would endanger users**. For sensitive reports, use a private GitHub security advisory once enabled for the repository.

Include:

- RouterOS version;
- architecture/model family (do not include serial number);
- Susanin version;
- minimal reproduction;
- sanitized log snippets;
- expected vs actual behavior.
