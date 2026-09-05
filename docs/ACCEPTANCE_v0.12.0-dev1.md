# Susanin v0.12.0-dev1 acceptance

Test platform:

- RouterOS 7.23.3
- ARM64
- production baseline: Susanin v0.11.5
- development controller isolated from production /data

## Result

v0.12.0-dev1 acceptance: PASS.

Validated:

- ARM64 Docker image and real AArch64 ELF;
- routing target mode: interface;
- routing target mode: routing-table;
- table-native HEALTH rendering;
- existing routing table `r_to_awg`;
- VPN Direct IPv4/CIDR persistence and synchronization;
- VPN Direct domain persistence;
- RouterOS DNS static FWD integration;
- `match-subdomain=yes`;
- VPN Direct bypass before all AUTO-AWG mangle rules;
- repeated `direct sync`;
- complete RouterOS cleanup after policy removal;
- production FAST/SOFT/JUDGE remained unchanged from v0.11.5.

## VPN Direct DNS behavior

RouterOS internal `:resolve` does not populate the DNS static
`address-list`.

An external DNS client querying the RouterOS DNS service does populate
the list.

Observed acceptance sequence:

1. `domain example.com` created one FWD rule;
2. internal RouterOS `:resolve` left dynamic `vpn_direct` count at zero;
3. an external DNS request through RouterOS populated two IPv4 entries;
4. `direct sync` cleared the dynamic entries;
5. a subsequent external DNS request repopulated them;
6. removing the domain removed the FWD rule, bypass rule and all
   `vpn_direct` entries.

Therefore Domain VPN Direct depends on DNS requests being visible to
RouterOS.

Clients using independent DNS, DoH, DoT, application-private DNS or a
hard-coded destination may not populate the RouterOS DNS-backed list.

SNI-assisted verification planned for later v0.12 development can
increase observability but cannot provide a universal hostname guarantee
for QUIC/ECH/private DNS.

## IPv4-only requirement

The acceptance DNS query returned both A and AAAA records.

Susanin manages only IPv4.

Before v0.12 is allowed into production, strict IPv4-only mode must be
active on RouterOS. IPv6 must not provide an unmanaged path around
Susanin.

Current setup logic sets:

    /ipv6/settings set disable-ipv6=yes

Susanin does not reboot RouterOS automatically.

## Production safety

After acceptance:

- stable controller: v0.11.5 running;
- development controller: stopped;
- VPN Direct mangle objects: 0;
- VPN Direct DNS objects: 0;
- VPN Direct address-list objects: 0;
- temporary DNS test firewall rules: 0.

Production source sizes remained:

- HEALTH: 4186
- FAST: 4041
- DETECT: 8075
- JUDGE: 6122
