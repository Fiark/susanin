# Susanin v0.12.0-dev3 acceptance

Test platform:

- MikroTik RouterOS 7.23.3
- ARM64
- production baseline: Susanin v0.11.5
- routing target: existing routing table `r_to_awg`
- egress interface: `wg-awg-proxy`
- strict IPv4-only RouterOS operation
- development controller isolated from production `/data`

## Result

v0.12.0-dev3 acceptance: **PASS**.

Accepted executable code commit:

    96277e3e6e2f38afe6cf8fb7124ae66c8c72ca87

Parent accepted MIDDLE implementation commit:

    3bb175f4fb891f917b4a47af106f4c033f38d35c

The development tag `v0.12.0-dev3` freezes the executable code
commit above. This acceptance document is intentionally committed as a
documentation-only child after the accepted executable commit.

## ARM64 candidate

Validated artifact:

    susanin-0.12.0-dev3-96277e3-arm64.tar

Artifact size:

    9083904 bytes

Artifact SHA256:

    a4198d4f45439285addf0f3d4d3b4403880002bd75b0abfc5d3b07e16c91aca7

Docker / RouterOS image ID:

    16151cfc4b33cbe9ad2afa9c12f4bb139de9eac260d32419287c7b821ef1406d

Runtime:

    Susanin 0.12.0-dev3

The image was built for `linux/arm64`, contained an AArch64 ELF
binary, passed a Docker save/load round trip, and the templates inside
the image matched the repository source.

## Final source blobs

The accepted executable commit contains:

    templates/fast.rsc.tmpl   4135da6640b7d2b206257da92ec68d7d1e453ca1
    templates/soft.rsc.tmpl   8ae4f65c39c25f9fc083257aef8bac97239dc175
    templates/judge.rsc.tmpl  f6aee1b937e43fd35cc3bf21e74d1bd14c9ad2bf

The accepted RouterOS 7.23.3 exact-mark JUDGE workaround remained
present. Accepted detector and JUDGE thresholds were not changed by
the final SLOW implementation.

## Accuracy profile acceptance

All three profiles were functionally validated on the same final
commit `96277e3e6e2f38afe6cf8fb7124ae66c8c72ca87`.

### FAST

Validated state machine:

    strong DIRECT failure
      -> TEST
      -> one healthy AWG test
      -> OK

Observed:

- one DIRECT TCP SYN failure was sufficient to enter TEST;
- no `direct1`, `awg1`, or `recheck` evidence was created;
- one healthy AWG TEST flow confirmed OK;
- the confirmed flow used AWG with
  `reply-dst-address=10.8.1.46`;
- `tcp/80`, `tcp/443`, and `udp/80` remained isolated.

Final FAST rendered RouterOS fingerprints:

    HEALTH  bytes=5774   fnv1a64=8abd7d7256f0be6a
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

### MIDDLE

Validated state machine:

    DIRECT evidence #1
      -> DIRECT
      -> independent DIRECT evidence #2
      -> TEST
      -> AWG evidence #1
      -> TEST
      -> independent AWG evidence #2
      -> OK

Observed:

- first DIRECT failure created `direct1` but did not enter TEST;
- the second independent DIRECT connection entered TEST;
- first healthy AWG evidence created `awg1` but did not create OK;
- the second independent healthy AWG connection confirmed OK;
- the confirmed flow used AWG with
  `reply-dst-address=10.8.1.46`;
- tuple isolation passed.

Final MIDDLE rendered RouterOS fingerprints:

    HEALTH  bytes=5776   fnv1a64=89b3925ff2660c57
    FAST    bytes=26100  fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521  fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842  fnv1a64=6654d7bbb164e505

### SLOW

DEV3 SLOW intentionally uses a DIRECT re-check. SNI-assisted
verification remains deferred to DEV4.

Validated state machine:

    DIRECT evidence #1
      -> DIRECT
      -> independent DIRECT evidence #2
      -> TEST
      -> first healthy AWG evidence
      -> DIRECT re-check
      -> failed independent DIRECT re-check
      -> TEST
      -> second independent healthy AWG evidence
      -> OK

Observed:

- first DIRECT failure was insufficient;
- second independent DIRECT failure entered TEST;
- first healthy AWG flow created both `awg1` and `recheck`;
- TEST was removed after the first AWG success;
- a subsequent healthy DIRECT flow remained unmarked and did not
  create OK;
- a new strong DIRECT SYN failure removed `recheck` and re-entered
  TEST while preserving `awg1`;
- the final independent healthy AWG flow confirmed OK;
- confirmed traffic used AWG;
- tuple isolation passed.

The first full SLOW harness was blocked after the healthy DIRECT
re-check because the test harness repeatedly selected a stale
`TIME-WAIT` connection rather than the new SYN evidence. Cleanup
completed successfully and no product-state failure was observed.
A continuation harness using a narrow `tcp-state=syn-sent` query
then validated the remaining
`RECHECK -> TEST -> AWG2 -> OK` transition.

Final SLOW rendered RouterOS fingerprints:

    HEALTH  bytes=5774   fnv1a64=b1ba0cb5c5ea267b
    FAST    bytes=26098  fnv1a64=e005143cae6e3828
    DETECT  bytes=41519  fnv1a64=6165a9bf88ed2537
    JUDGE   bytes=16840  fnv1a64=25d83785b79b9661

## Identity isolation

The adaptive identity remained:

    protocol + destination IPv4 + destination port

Acceptance explicitly verified that the decision for:

    tcp / 1.1.1.1 / 80

did not propagate to:

    tcp / 1.1.1.1 / 443
    udp / 1.1.1.1 / 80

## RouterOS final closure

Final closure passed with:

    production source lengths = 4186 / 4041 / 8075 / 6122
    production schedulers     = 4 / 4
    fixed mangle rules        = 8 / 8
    stage schedulers          = 0
    promotion backups         = 0
    dev3 direct1 residue      = 0
    dev3 awg1 residue         = 0
    dev3 recheck residue      = 0
    dev3 lazy-rule residue    = 0
    AWG infrastructure        = 1 / 1 / 1
    disable-ipv6              = true

The final inert stage was MIDDLE with:

    HEALTH  bytes=5776   fnv1a64=89b3925ff2660c57
    FAST    bytes=26100  fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521  fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842  fnv1a64=6654d7bbb164e505

No DEV3 promotion was performed.

Final controller state:

- stable `susanin-controller` v0.11.5: RUNNING;
- final DEV3 `susanin-controller-v012dev3-96277e3`: STOPPED;
- accepted MIDDLE baseline
  `susanin-controller-v012dev3-3bb175f`: STOPPED;
- DEV1 traffic generator: STOPPED.

The independent AWG container/interface/routing-table/NAT
infrastructure remained intact.

## DEV3 conclusion

DEV3 acceptance is complete for the FAST, MIDDLE and SLOW evidence
state machines on RouterOS 7.23.3 / ARM64.

Further v0.12 work, including SNI-assisted verification, migration,
garbage collection and longer-running acceptance, remains DEV4 work.
