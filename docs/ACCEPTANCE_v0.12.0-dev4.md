# Susanin v0.12.0-dev4 acceptance worklog

Status:

    PARTIAL — migration hardening in progress

Reference platform:

- MikroTik RouterOS 7.23.3 stable;
- ARM64;
- production baseline Susanin v0.11.5;
- routing target `r_to_awg`;
- egress interface `wg-awg-proxy`;
- strict IPv4-only operation.

DEV4 executable under test:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The executable logic at this commit is the accepted DEV3 executable logic
plus the DEV4 version string. SNI is explicitly outside the v0.12 release
scope.

## M1 — v0.11.5 -> DEV4 FAST -> rollback

Result:

    PASS

### Starting production

Exact stable production source lengths:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

Runtime gates:

    managed schedulers enabled = 4 / 4
    fixed mangle rules enabled = 8 / 8
    AWG infrastructure         = 1 / 1 / 1

Independent AWG objects remained outside Susanin ownership.

### DEV4 FAST stage

Rendered and staged source:

    HEALTH  bytes=5774   fnv1a64=8abd7d7256f0be6a
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

Stage verification passed before write-mode promotion.

### Controlled legacy fixture

Eight explicit v0.11.5 IP-only state entries were added:

    auto_awg_watch_tcp
    auto_awg_test_tcp
    auto_awg_ok_tcp
    auto_awg_cooldown_tcp
    auto_awg_watch_udp
    auto_awg_test_udp
    auto_awg_ok_udp
    auto_awg_cooldown_udp

Controlled connection-mark fixture evidence:

    TEST rule packets = 1
    OK rule packets   = 3

Conntrack before promotion:

    fixture TEST marks = 1
    fixture OK marks   = 2

The fixture therefore proved that real adaptive TEST/OK marks existed before
migration.

### Promotion cleanup

Promotion completed successfully.

Observed compatibility cleanup:

    dev2 lazy rules:
        initial=0
        remaining=0

    dev2 port/profile lists:
        initial=0
        remaining=0

    legacy IP-only lists:
        initial=10
        remaining=0

    adaptive TEST/OK connection marks:
        initial=6
        remaining=0
        verification attempts=5

The cleanup counts were intentionally not limited to the controlled fixture.

The fixture contained eight legacy entries, while RouterOS contained ten
legacy entries in total. Likewise the fixture demonstrated three marked
connections while six adaptive marked connections existed in the live
conntrack table.

This is an accepted result: migration conservatively clears the complete
incompatible adaptive namespace rather than only state created by the test.

### Promoted production

Exact source lengths after promotion:

    5774 / 26098 / 41519 / 16840

Post-promotion:

    legacy fixture residue       = 0
    controlled TEST mark residue = 0
    controlled OK mark residue   = 0
    standard stage objects       = 0
    schedulers enabled           = 4 / 4

Four persistent rollback backups were created.

They contained the exact previous v0.11.5 production sources:

    4186 / 4041 / 8075 / 6122

### Product rollback

`SUSANIN ROLLBACK v0.12.0-dev4` completed successfully.

Rollback cleanup reached zero incompatible runtime state.

Exact restored production sources:

    HEALTH  bytes=4186  fnv1a64=4c7dd5b339ae022e
    FAST    bytes=4041  fnv1a64=ec9f4c35b9620fe8
    DETECT  bytes=8075  fnv1a64=9630197877300715
    JUDGE   bytes=6122  fnv1a64=65c7027f14ae3c48

Schedulers were restored to:

    4 / 4 enabled

### Final closure

Final reference-router state:

    production sources = 4186 / 4041 / 8075 / 6122
    schedulers          = 4 / 4
    fixed mangle        = 8 / 8
    AWG                 = 1 / 1 / 1

Test residue:

    fixture address-list entries = 0
    fixture mangle rules         = 0
    DEV3 stage holds             = 0
    rollback backups             = 0
    E2E safety backups           = 0

Accepted DEV3 MIDDLE inert stage was restored:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

## M1 conclusion

M1 is accepted.

The tested v0.12 migration path successfully:

1. staged and verified the desired DEV4 FAST source;
2. created exact rollback backups;
3. paused managed schedulers;
4. promoted all four production sources;
5. removed incompatible legacy v0.11.5 state;
6. removed live adaptive TEST/OK connection marks;
7. restored scheduler state;
8. rolled back to exact v0.11.5 sources;
9. left independent AWG infrastructure unchanged.

Next migration acceptance case:

    M2 — FAST -> MIDDLE
