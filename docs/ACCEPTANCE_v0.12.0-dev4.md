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

## M2 — FAST -> MIDDLE -> rollback FAST

Result:

    PASS

M2 was executed on the same RouterOS 7.23.3 ARM64 reference router and with
the same DEV4 executable:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The test began and ended with the exact stable v0.11.5 reference state.

### Reference -> FAST setup

DEV4 was configured for:

    accuracy profile = fast
    target mode      = routing-table
    routing table    = r_to_awg

Exact FAST stage and production source:

    HEALTH  bytes=5774   fnv1a64=8abd7d7256f0be6a
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

Reference -> FAST promotion:

    PASS

The setup promotion also exercised cleanup against live stable runtime:

    legacy IP-only entries         initial=3 -> remaining=0
    adaptive connection marks      initial=3 -> remaining=0
    connection-mark verify attempts=6

### MIDDLE desired source

DEV4 was switched to the MIDDLE accuracy profile.

Exact MIDDLE staged source:

    HEALTH  bytes=5776   fnv1a64=89b3925ff2660c57
    FAST    bytes=26100  fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521  fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842  fnv1a64=6654d7bbb164e505

Stage verification passed before write-mode promotion.

### FAST runtime fixture

Eight controlled tuple-aware address-list entries were initially created:

    auto_awg_watch_tcp_443
    auto_awg_test_tcp_443
    auto_awg_ok_tcp_443
    auto_awg_cooldown_tcp_443
    auto_awg_watch_udp_443
    auto_awg_test_udp_443
    auto_awg_ok_udp_443
    auto_awg_cooldown_udp_443

Initial fixture count:

    8

Two controlled lazy rules were created:

    AUTO-AWG: P tcp 443 OK
    AUTO-AWG: P tcp 443 TEST

Initial lazy-rule count:

    2

Controlled conntrack evidence was also produced.

Fixture rule counters:

    TEST packets = 1
    OK packets   = 2

Visible controlled connection marks before migration:

    TEST = 1
    OK   = 1

This proves that real tuple state, lazy mark rules and TEST/OK connection
marks existed before FAST -> MIDDLE promotion.

### FAST -> MIDDLE compatibility cleanup

Promotion completed successfully.

Observed cleanup:

    dev2 lazy rules:
        initial=2
        remaining=0
        verification attempts=1

    dev2 port/profile lists:
        initial=4
        remaining=0
        verification attempts=1

    legacy IP-only lists:
        initial=0
        remaining=0

    adaptive TEST/OK connection marks:
        initial=3
        remaining=0
        verification attempts=2

Eight tuple fixture entries were created initially. At the compatibility
cleanup scan four matching tuple entries remained live; the remaining state
had already changed/disappeared before the cleanup scan. The authoritative
post-condition was complete namespace absence after migration.

Post-promotion validation:

    controlled fixture entries = 0
    tuple-aware runtime         = 0
    lazy rules                  = 0
    adaptive TEST/OK marks      = 0
    standard stage objects      = 0
    managed schedulers enabled  = 4 / 4

Exact MIDDLE production source:

    5776 / 26100 / 41521 / 16842

Four rollback backups were retained and contained the exact previous FAST
source:

    5774 / 26098 / 41519 / 16840

### Product rollback MIDDLE -> FAST

`SUSANIN ROLLBACK v0.12.0-dev4` completed successfully.

Rollback runtime cleanup reached zero incompatible state.

Exact restored FAST production:

    HEALTH  5774
    FAST    26098
    DETECT  41519
    JUDGE   16840

Managed schedulers after rollback:

    4 / 4 enabled

Result:

    PASS

### Reference-router recovery

After M2, the independent E2E safety copies were used to restore the exact
stable v0.11.5 reference source:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

Managed script jobs were confirmed idle before source restoration.

Final reference-router gates:

    production sources = 4186 / 4041 / 8075 / 6122
    schedulers          = 4 / 4
    fixed mangle        = 8 / 8
    AWG                 = 1 / 1 / 1

Runtime residue:

    tuple-aware state = 0
    lazy rules        = 0
    DEV3 stage holds  = 0
    rollback backups  = 0
    E2E safety copies = 0

Accepted DEV3 MIDDLE inert stage restored:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

Temporary RouterOS M2 harness:

    removed

### M2 conclusion

M2 is accepted.

The test proves that a profile transition from FAST to MIDDLE does not reuse
FAST tuple/runtime evidence as MIDDLE evidence. Promotion clears incompatible
tuple-aware state, lazy rules and adaptive connection marks before managed
scheduler state is restored.

The product rollback path also restored the exact pre-promotion FAST source.

## M3 — MIDDLE -> SLOW -> rollback MIDDLE

Result:

    PASS

M3 was executed on MikroTik RouterOS 7.23.3 / ARM64 with the same DEV4
executable:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The reference router began and ended on exact stable v0.11.5 production.

### Reference -> MIDDLE setup

DEV4 was configured for the MIDDLE accuracy profile and promoted from the
stable reference.

Exact MIDDLE source:

    HEALTH  bytes=5776   fnv1a64=89b3925ff2660c57
    FAST    bytes=26100  fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521  fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842  fnv1a64=6654d7bbb164e505

Reference -> MIDDLE:

    PASS

The setup promotion also cleaned live stable runtime state:

    legacy IP-only lists:
        initial=2
        remaining=0

    adaptive connection marks:
        initial=5
        remaining=0
        verification attempts=5

### SLOW desired source

DEV4 was switched to the SLOW accuracy profile.

Exact SLOW stage:

    HEALTH  bytes=5774   fnv1a64=b1ba0cb5c5ea267b
    FAST    bytes=26098  fnv1a64=e005143cae6e3828
    DETECT  bytes=41519  fnv1a64=6165a9bf88ed2537
    JUDGE   bytes=16840  fnv1a64=25d83785b79b9661

All four staged objects matched the accepted DEV3 SLOW executable logic.

### MIDDLE runtime fixture

Controlled normal tuple state was created for TCP and UDP:

    WATCH
    TEST
    OK
    COOLDOWN

Controlled MIDDLE evidence was also created:

    DIRECT1 tcp
    AWG1    tcp
    DIRECT1 udp
    AWG1    udp

Total controlled address-list fixture:

    12

Evidence counts:

    DIRECT1 = 2
    AWG1    = 2

SLOW-only RECHECK state before migration:

    0

Two controlled lazy mark rules were created for tcp/54321:

    AUTO-AWG: P tcp 54321 OK
    AUTO-AWG: P tcp 54321 TEST

Lazy-rule fixture count:

    2

Controlled live connection-mark evidence was produced.

Mangle counters:

    TEST packets = 1
    OK packets   = 2

Visible controlled conntrack marks before migration:

    TEST = 1
    OK   = 2

### MIDDLE -> SLOW compatibility cleanup

Promotion completed successfully.

Observed cleanup:

    dev2 lazy rules:
        initial=2
        remaining=0
        verification attempts=1

    dev2 port/profile lists:
        initial=12
        remaining=0
        verification attempts=1

    legacy IP-only lists:
        initial=0
        remaining=0

    adaptive TEST/OK connection marks:
        initial=3
        remaining=0
        verification attempts=5

During live conntrack verification RouterOS 7.23.3 returned two transient:

    no such item (4)

API errors.

The bounded connection-scan retry path handled these transient live-conntrack
races and still reached the authoritative post-condition:

    remaining=0

This is accepted evidence that the DEV4 bounded retry implementation survives
the RouterOS 7.23.3 dynamic conntrack serialization race without an unbounded
loop and without leaving incompatible adaptive state.

### SLOW production post-condition

Exact promoted SLOW source:

    5774 / 26098 / 41519 / 16840

Rollback backups contained exact previous MIDDLE source:

    5776 / 26100 / 41521 / 16842

After promotion:

    controlled address-list fixture = 0
    controlled lazy rules           = 0
    controlled TEST mark            = 0
    controlled OK mark              = 0
    standard stage objects          = 0
    managed schedulers enabled      = 4 / 4

The controlled MIDDLE DIRECT1/AWG1 addresses were explicitly checked against
the SLOW RECHECK namespace.

Result:

    MIDDLE evidence reused as RECHECK = 0

Therefore MIDDLE evidence was not silently reused as SLOW re-check evidence.

### Product rollback SLOW -> MIDDLE

`SUSANIN ROLLBACK v0.12.0-dev4` completed successfully.

Rollback runtime cleanup reached zero incompatible state.

Exact restored MIDDLE production:

    HEALTH  bytes=5776  fnv1a64=89b3925ff2660c57
    FAST    bytes=26100 fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521 fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842 fnv1a64=6654d7bbb164e505

Managed schedulers:

    4 / 4 enabled

Result:

    PASS

### Reference-router recovery

Managed schedulers were paused and managed jobs were confirmed idle before
restoring the exact v0.11.5 safety copies.

Exact final production:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

Final reference-router gates:

    production sources = 4186 / 4041 / 8075 / 6122
    schedulers          = 4 / 4
    fixed mangle        = 8 / 8
    AWG                 = 1 / 1 / 1

Final adaptive residue:

    port-aware state = 0
    lazy rules       = 0
    DEV3 stage holds = 0
    rollback backups = 0
    E2E safety copies= 0

Accepted DEV3 MIDDLE inert stage restored:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

Temporary RouterOS M3 harness:

    removed

### M3 conclusion

M3 is accepted.

The test proves that MIDDLE -> SLOW migration clears normal tuple state,
MIDDLE DIRECT1/AWG1 evidence, lazy rules and adaptive TEST/OK connection
marks before SLOW resumes.

MIDDLE evidence was not reused as SLOW RECHECK evidence.

The test also provides physical RouterOS 7.23.3 evidence that bounded
conntrack retry survives transient `no such item` races and converges to the
required zero-state post-condition.

The product rollback path restored the exact previous MIDDLE source.

## M4 — SLOW -> FAST -> rollback SLOW

Result:

    PASS

M4 was executed on MikroTik RouterOS 7.23.3 / ARM64 using the same DEV4
executable:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The test began and ended with exact stable v0.11.5 production.

### Reference -> SLOW setup

DEV4 was configured for the SLOW accuracy profile.

Exact SLOW desired source:

    HEALTH  bytes=5774   fnv1a64=b1ba0cb5c5ea267b
    FAST    bytes=26098  fnv1a64=e005143cae6e3828
    DETECT  bytes=41519  fnv1a64=6165a9bf88ed2537
    JUDGE   bytes=16840  fnv1a64=25d83785b79b9661

Reference -> SLOW promotion completed successfully.

Setup compatibility cleanup also cleared live stable runtime:

    legacy IP-only lists:
        initial=2
        remaining=0
        verification attempts=1

    adaptive TEST/OK connection marks:
        initial=2
        remaining=0
        verification attempts=1

Because FAST and SLOW have identical rendered source lengths, M4 did not use
length alone as the profile identity check.

A fresh SLOW stage was rendered after promotion and every production source
was compared directly against the desired SLOW stage source.

Result:

    SLOW source equality = 4 / 4

The verification stage was then removed successfully.

### FAST desired source

DEV4 was switched to the FAST profile.

Exact FAST desired source:

    HEALTH  bytes=5774   fnv1a64=8abd7d7256f0be6a
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

The FAST fingerprints explicitly distinguish FAST from SLOW despite their
identical source lengths.

### SLOW runtime fixture

Controlled normal tuple state was created for TCP and UDP:

    WATCH
    TEST
    OK
    COOLDOWN

Controlled SLOW evidence was created for both protocols:

    DIRECT1
    AWG1
    RECHECK

Total controlled address-list fixture:

    14

Evidence counts:

    DIRECT1 = 2
    AWG1    = 2
    RECHECK = 2

Two lazy rules were created for tcp/54322:

    AUTO-AWG: P tcp 54322 OK
    AUTO-AWG: P tcp 54322 TEST

Lazy-rule fixture:

    2

Controlled live TEST/OK marks were also generated.

Mangle fixture counters:

    TEST packets = 1
    OK packets   = 3

Visible controlled conntrack marks before migration:

    TEST = 1
    OK   = 1

### SLOW -> FAST compatibility cleanup

Promotion completed successfully.

Observed cleanup:

    dev2 lazy rules:
        initial=2
        remaining=0
        verification attempts=1

    dev2 port/profile lists:
        initial=14
        remaining=0
        verification attempts=1

    legacy IP-only lists:
        initial=0
        remaining=0

    adaptive TEST/OK connection marks:
        initial=4
        remaining=0
        verification attempts=7

The connection-mark cleanup therefore remained bounded and converged to the
required zero-state post-condition.

Four rollback backups were created.

Their comments contained the exact previous SLOW fingerprints:

    HEALTH  b1ba0cb5c5ea267b
    FAST    e005143cae6e3828
    DETECT  6165a9bf88ed2537
    JUDGE   25d83785b79b9661

Result:

    rollback backup profile = SLOW

### Exact FAST production validation

A fresh FAST stage was rendered after promotion and compared directly with
all four production sources.

Result:

    FAST source equality = 4 / 4

The temporary verification stage was then removed.

Post-migration state:

    controlled SLOW fixture       = 0
    tcp/54322 lazy rules          = 0
    DIRECT1/AWG1/RECHECK evidence = 0
    controlled TEST mark          = 0
    controlled OK mark            = 0
    managed schedulers            = 4 / 4 enabled

Therefore SLOW-specific evidence does not survive or become FAST evidence.

### Product rollback FAST -> SLOW

`SUSANIN ROLLBACK v0.12.0-dev4` completed successfully.

Rollback restored:

    HEALTH  5774 / b1ba0cb5c5ea267b
    FAST    26098 / e005143cae6e3828
    DETECT  41519 / 6165a9bf88ed2537
    JUDGE   16840 / 25d83785b79b9661

DEV4 was switched back to the SLOW profile and a fresh SLOW stage was rendered
for direct source comparison.

Result:

    SLOW rollback source equality = 4 / 4

Managed schedulers after rollback:

    4 / 4 enabled

Result:

    PASS

### Reference-router recovery

Managed schedulers were paused and all managed jobs were confirmed idle.

Exact stable v0.11.5 safety sources were restored:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

DEV4 isolated configuration was reset to:

    accuracy profile = fast

Final reference-router gates:

    production sources = 4186 / 4041 / 8075 / 6122
    schedulers          = 4 / 4
    fixed mangle        = 8 / 8
    AWG                 = 1 / 1 / 1

Final adaptive residue:

    port-aware state = 0
    lazy rules       = 0
    DEV3 stage holds = 0
    rollback backups = 0
    E2E safety copies= 0

Accepted DEV3 MIDDLE inert stage restored:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

Temporary RouterOS M4 harness:

    removed

### M4 conclusion

M4 is accepted.

The SLOW -> FAST transition clears the complete incompatible SLOW runtime,
including normal tuple state, DIRECT1, AWG1, RECHECK, lazy rules and adaptive
connection marks.

Direct desired-source equality checks prove that the resulting profile is
FAST and not SLOW despite identical rendered source lengths.

Product rollback restored the exact previous SLOW profile.

With M4 complete, the basic migration transition matrix is now accepted:

    v0.11.5 -> FAST
    FAST    -> MIDDLE
    MIDDLE  -> SLOW
    SLOW    -> FAST

## M5 — same-profile re-promotion

Result:

    PASS

M5 was executed on MikroTik RouterOS 7.23.3 / ARM64 using the same DEV4
executable:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The test exercised:

    v0.11.5 -> FAST -> FAST -> reference v0.11.5

The second promotion used the exact same generated FAST source already
installed in production.

### Reference -> FAST setup

Exact FAST desired source:

    HEALTH  bytes=5774   fnv1a64=8abd7d7256f0be6a
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

The first promotion completed successfully.

Exact resulting production source:

    5774 / 26098 / 41519 / 16840

The first promotion created exactly four rollback backups.

Their source was the exact previous stable v0.11.5 production:

    4186 / 4041 / 8075 / 6122

No stage objects remained after the first successful promotion.

The setup cleanup also encountered one transient RouterOS 7.23.3:

    no such item (4)

while clearing live adaptive connection marks.

The bounded cleanup converged successfully:

    adaptive marks:
        initial=2
        remaining=0
        verification attempts=5

### Exact same-profile precondition

A fresh FAST stage was generated while production was already FAST.

Every production source was compared directly against its desired staged
source.

Result:

    same-profile source equality before promotion = 4 / 4

Therefore the second promotion was a genuine exact-source FAST -> FAST
re-promotion rather than another profile transition.

### Controlled runtime before re-promotion

Four controlled tuple-aware FAST entries were created for tcp/54400:

    WATCH
    TEST
    OK
    COOLDOWN

Initial fixture count:

    4

Two controlled lazy rules were created:

    AUTO-AWG: P tcp 54400 OK
    AUTO-AWG: P tcp 54400 TEST

Initial lazy-rule count:

    2

Same-profile promotion is allowed to conservatively forget this adaptive
runtime.

### FAST -> FAST re-promotion

The second `promote` completed successfully.

Observed compatibility cleanup:

    dev2 lazy rules:
        initial=2
        remaining=0
        verification attempts=1

    dev2 port/profile lists:
        initial=2
        remaining=0
        verification attempts=1

    legacy IP-only lists:
        initial=0
        remaining=0

    adaptive connection marks:
        initial=0
        remaining=0

Four tuple entries were created initially. Two matching tuple entries
remained live at the product cleanup scan. The authoritative post-condition
after promotion was complete absence of the controlled runtime.

Post-condition:

    controlled tuple fixture = 0
    controlled lazy rules    = 0
    standard stage objects   = 0

### Backup idempotence

After the first promotion:

    rollback backup objects = 4

After the second promotion:

    rollback backup objects = 4

The exact backup names remained:

    susanin-backup-health
    susanin-backup-fast
    susanin-backup-detect
    susanin-backup-judge

No additional backup objects accumulated.

The second promotion replaced the rollback set with the immediately previous
production source, which was FAST.

Exact replacement backup fingerprints:

    HEALTH  8abd7d7256f0be6a
    FAST    0c9672d93a6a4e85
    DETECT  0ee9c8e6708bc6e8
    JUDGE   72733543f4561160

Result:

    rollback backup profile = FAST

This proves that rollback state tracks the immediately previous production
rather than preserving or accumulating older backup generations.

### Managed topology after second promotion

Exact topology:

    managed production scripts     = 4 / 4
    managed production schedulers  = 4 / 4
    managed schedulers enabled     = 4 / 4
    rollback backup objects        = 4
    stale standard stage objects   = 0

Independent AWG topology:

    wg-awg-proxy                       = 1
    r_to_awg                           = 1
    AWG selected traffic masquerade    = 1

Result:

    unchanged

No managed script or scheduler was duplicated.

### Exact production verification

After the second promotion a fresh FAST stage was generated.

All four production sources were compared directly against the newly
generated desired source.

Result:

    same-profile source equality after promotion = 4 / 4

The verification stage was then removed successfully.

Therefore same-profile promotion did not alter the desired FAST source.

### Reference-router recovery

Managed schedulers were paused and managed script jobs reached:

    jobs-idle=true

Exact stable v0.11.5 sources were restored:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

Final reference-router gates:

    production sources = 4186 / 4041 / 8075 / 6122
    schedulers          = 4 / 4
    fixed mangle        = 8 / 8
    AWG                 = 1 / 1 / 1

Final adaptive/lifecycle residue:

    port-aware state = 0
    lazy rules       = 0
    DEV3 stage holds = 0
    rollback backups = 0
    E2E safety copies= 0

Accepted DEV3 MIDDLE inert stage restored:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

Temporary RouterOS M5 harness:

    removed

### M5 conclusion

M5 is accepted.

Promoting exact same-profile FAST sources again is safe and conservative.

It does not:

- duplicate managed production scripts;
- duplicate managed schedulers;
- accumulate rollback backup objects;
- preserve stale stage objects;
- alter independent AWG infrastructure.

The rollback set is replaced with exactly four backups representing the
immediately previous FAST production.

Controlled adaptive runtime is conservatively cleared, and production remains
byte-for-byte equal to the desired FAST source after re-promotion.

## RouterOS 7.23.3 HEALTH regression found during M6

Status:

    FIXED AND PHYSICALLY VERIFIED

The first M6 profile-switch repetition attempt did not expose a migration
transaction failure.

The first FAST -> MIDDLE transition itself completed successfully:

    desired MIDDLE stage        = exact
    promotion result            = SUCCESS
    rollback backup profile     = FAST
    controlled runtime residue  = 0
    desired production equality = 4 / 4
    stage residue               = 0

The M6 topology gate then observed:

    fixed AUTO-AWG mangle enabled = 0 / 8

Reference recovery returned the router to the exact stable v0.11.5 state.

### Root cause isolation

A separate read-only RouterOS 7.23.3 probe comparison established:

Stable v0.11.5 health method:

    /ping ... interface=wg-awg-proxy src-address=10.8.1.46

Result:

    1.1.1.1 = 3 / 3
    8.8.8.8 = 3 / 3
    total   = 6 / 6

DEV4 ca8f422 health method:

    /ping ... routing-table=r_to_awg

RouterOS 7.23.3 rejected that syntax:

    bad parameter routing-table

Result:

    1.1.1.1 = 0 / 3
    8.8.8.8 = 0 / 3
    total   = 0 / 6

The routing table itself still had its active default route through:

    wg-awg-proxy

RouterOS logs also recorded the resulting false fail-open event:

    AUTO-AWG: routing target DOWN after 2 health misses, fallback to DIRECT

Therefore the M6 `fixed=0/8` gate was valid and exposed a real DEV4 HEALTH
regression.

The migration harness was not weakened.

### Source fix

The superseded executable candidate was:

    ca8f422bc005b2f5702c036ad18bf3758ea114b8

The fix was committed as:

    490eb839cb42551707b6640bd20f7a5d29484f5b
    Fix RouterOS 7.23.3 health probe

The fix:

- removes unsupported RouterOS 7.23.3 `/ping routing-table=...`;
- resolves and retains the concrete egress interface for HEALTH;
- probes using the egress interface and its IPv4 source address;
- blocks rendering when no concrete IPv4 HEALTH egress can be resolved;
- leaves adaptive packet routing table-native.

Strict native build passed with:

    -Os -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror

### Replacement ARM64 candidate

Exact candidate:

    commit:
        490eb839cb42551707b6640bd20f7a5d29484f5b

    image:
        27e59450fdcc973ad32388be3390bfe9c79408998c3502e3081328198a6df35c

    tar size:
        9083904

    tar sha256:
        1eb027c1f6ad22be51c87e004a6287627664605827c4e9147c8569dc6363f0e4

The ARM64 binary and runtime templates inside the image were verified before
RouterOS import.

The image was imported on the RouterOS 7.23.3 ARM64 reference router with
isolated data storage:

    /susanin-data-v012dev4-490eb83 -> /data

Stable production remained untouched during import.

### Physical HEALTH regression test

The replacement candidate resolved the selected routing table to:

    table  = r_to_awg
    egress = wg-awg-proxy

Exact new FAST stage:

    HEALTH  bytes=6148   fnv1a64=cafdf828c49d2946
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

Rendered HEALTH source verification:

    unsupported routing-table ping = absent
    interface/src-address probe    = present
    wg-awg-proxy                   = present

The exact newly rendered HEALTH source was copied into a temporary inert test
script.

The stable v0.11.5 HEALTH scheduler was paused during the probe sequence so it
could not mask the result.

Six consecutive executions were performed.

Every run reached both configured external health addresses and finished with:

    fixed AUTO-AWG mangle = 8 / 8 enabled
    auto_awg_health_fail  = 0

Aggregate result:

    exact new HEALTH runs = 6 / 6 PASS
    false health misses   = 0
    false fail-open       = 0

### Reference recovery

After the HEALTH regression test:

    production source = 4186 / 4041 / 8075 / 6122
    schedulers        = 4 / 4
    fixed mangle      = 8 / 8
    AWG               = 1 / 1 / 1

Accepted DEV3 MIDDLE inert stage was restored:

    5776 / 26100 / 41521 / 16842

Test residue:

    DEV3 stage holds        = 0
    temporary HEALTH script = 0
    health fail-list        = 0

Controller state:

    stable v0.11.5       = RUNNING
    ca8f422              = STOPPED / superseded
    490eb83              = STOPPED / current candidate

### HEALTH regression conclusion

The RouterOS 7.23.3 HEALTH regression is accepted as fixed.

The physical test proves that the replacement candidate no longer falsely
declares the working `r_to_awg` / `wg-awg-proxy` target down due to unsupported
RouterOS ping syntax.

The previous candidate `ca8f422` is superseded.

The current executable candidate for the remaining DEV4 acceptance work is:

    490eb839cb42551707b6640bd20f7a5d29484f5b

M6 itself remains pending and must be repeated against this replacement
candidate with the original fixed-mangle safety gate retained.

## M6 — profile-switch repetition

Result:

    PASS

M6 V2 was executed on MikroTik RouterOS 7.23.3 / ARM64 using the replacement
DEV4 executable:

    490eb839cb42551707b6640bd20f7a5d29484f5b

ARM64 image:

    27e59450fdcc973ad32388be3390bfe9c79408998c3502e3081328198a6df35c

The test intentionally retained the original fixed-mangle safety gate which
had exposed the earlier RouterOS 7.23.3 HEALTH regression.

The bounded sequence was:

    stable v0.11.5 -> FAST
    FAST           -> MIDDLE
    MIDDLE         -> SLOW
    SLOW           -> FAST
    FAST           -> MIDDLE

The initial stable -> FAST operation was a setup transition.

The four following transitions are the M6 repeated profile-switch sequence.

### Updated 490eb83 desired-source identities

FAST:

    HEALTH  bytes=6148   fnv1a64=cafdf828c49d2946
    FAST    bytes=26098  fnv1a64=0c9672d93a6a4e85
    DETECT  bytes=41519  fnv1a64=0ee9c8e6708bc6e8
    JUDGE   bytes=16840  fnv1a64=72733543f4561160

MIDDLE:

    HEALTH  bytes=6150   fnv1a64=fe1c4593467d5283
    FAST    bytes=26100  fnv1a64=a6df6efc895000b4
    DETECT  bytes=41521  fnv1a64=c31920dc5302d7db
    JUDGE   bytes=16842  fnv1a64=6654d7bbb164e505

SLOW:

    HEALTH  bytes=6148   fnv1a64=f912cdff44de2a8f
    FAST    bytes=26098  fnv1a64=e005143cae6e3828
    DETECT  bytes=41519  fnv1a64=6165a9bf88ed2537
    JUDGE   bytes=16840  fnv1a64=25d83785b79b9661

The HEALTH source in every rendered target profile was also checked for the
RouterOS 7.23.3 fix:

    unsupported /ping routing-table=  = absent
    interface/src-address HEALTH      = present

Every target bundle contained its requested accuracy-profile sentinel in all
four generated scripts:

    target profile sentinel = 4 / 4

### Reference -> FAST setup

Exact replacement-candidate FAST stage:

    6148 / 26098 / 41519 / 16840

FAST HEALTH fingerprint:

    cafdf828c49d2946

The fixed HEALTH render check passed before promotion.

Reference -> FAST promotion completed successfully.

Rollback backup objects:

    4

The setup rollback source was the exact stable v0.11.5 production:

    4186 / 4041 / 8075 / 6122

A fresh FAST stage was then rendered and compared directly with production:

    setup FAST source equality = 4 / 4

The production HEALTH script was executed after setup.

Post-condition:

    fixed AUTO-AWG mangle = 8 / 8
    health fail-list      = 0

Result:

    reference -> FAST setup = PASS

### M6 transition method

Before every profile transition, M6 V2 created four temporary immutable
snapshots of the exact current production source:

    susanin-m6-prev-health
    susanin-m6-prev-fast
    susanin-m6-prev-detect
    susanin-m6-prev-judge

After promotion, every persistent rollback backup was compared byte-for-byte
against that immediate previous production snapshot.

This avoids relying on historical hard-coded fingerprints and proves the
semantic rollback invariant directly.

Required result after every transition:

    rollback backup objects              = 4
    immediate previous backup equality   = 4 / 4

The temporary previous-source snapshots were then removed.

### Controlled previous-profile runtime

Each transition also received deterministic previous-profile runtime before
promotion.

FAST previous profile:

    TEST tuple fixture = 1
    lazy rule          = 1

MIDDLE previous profile:

    TEST    = 1
    DIRECT1 = 1
    AWG1    = 1
    total tuple fixture = 3
    lazy rule           = 1

SLOW previous profile:

    TEST    = 1
    DIRECT1 = 1
    AWG1    = 1
    RECHECK = 1
    total tuple fixture = 4
    lazy rule           = 1

After every promotion:

    controlled previous-profile fixture = 0
    controlled lazy rule                = 0
    standard stage objects              = 0

The product cleanup scans were allowed to observe additional live scheduler
state.  The authoritative requirement was removal of the incompatible
controlled previous-profile state and successful post-promotion topology
verification.

### Step 1 — FAST -> MIDDLE

Desired source:

    HEALTH  6150 / fe1c4593467d5283
    FAST    26100 / a6df6efc895000b4
    DETECT  41521 / c31920dc5302d7db
    JUDGE   16842 / 6654d7bbb164e505

Promotion result:

    SUCCESS

Observed cleanup scan:

    lazy rules:
        initial=3
        remaining=0
        verification attempts=1

    port-aware lists:
        initial=2
        remaining=0
        verification attempts=1

Rollback invariant:

    rollback backups                    = 4 / 4
    immediate previous backup equality = 4 / 4
    previous profile                    = FAST

Post-promotion:

    controlled fixture          = 0
    controlled lazy rule        = 0
    stage                       = 0
    production source equality  = 4 / 4

Production HEALTH probe reached both configured external targets.

Topology gate:

    managed scripts             = 1 / 1 / 1 / 1
    managed schedulers          = 1 / 1 / 1 / 1
    managed schedulers enabled  = 4 / 4
    fixed mangle                = 8 / 8
    health fail-list            = 0
    rollback backups            = 4 / 4
    DEV3 holds                  = 4 / 4
    stable safety copies        = 4 / 4
    previous-source snapshots   = 0
    stage                       = 0
    controlled fixture          = 0
    AWG                         = 1 / 1 / 1

Result:

    PASS

### Step 2 — MIDDLE -> SLOW

Desired source:

    HEALTH  6148 / f912cdff44de2a8f
    FAST    26098 / e005143cae6e3828
    DETECT  41519 / 6165a9bf88ed2537
    JUDGE   16840 / 25d83785b79b9661

Promotion result:

    SUCCESS

Observed cleanup scan:

    lazy rules:
        initial=1
        remaining=0
        verification attempts=1

    port-aware lists:
        initial=3
        remaining=0
        verification attempts=1

Rollback invariant:

    rollback backups                    = 4 / 4
    immediate previous backup equality = 4 / 4
    previous profile                    = MIDDLE

Post-promotion:

    controlled fixture          = 0
    controlled lazy rule        = 0
    stage                       = 0
    production source equality  = 4 / 4

Production HEALTH and topology gate:

    fixed mangle       = 8 / 8
    health fail-list   = 0
    AWG                = 1 / 1 / 1

Result:

    PASS

### Step 3 — SLOW -> FAST

Desired source:

    HEALTH  6148 / cafdf828c49d2946
    FAST    26098 / 0c9672d93a6a4e85
    DETECT  41519 / 0ee9c8e6708bc6e8
    JUDGE   16840 / 72733543f4561160

Promotion result:

    SUCCESS

Observed cleanup scan:

    lazy rules:
        initial=1
        remaining=0
        verification attempts=1

    port-aware lists:
        initial=6
        remaining=0
        verification attempts=1

Rollback invariant:

    rollback backups                    = 4 / 4
    immediate previous backup equality = 4 / 4
    previous profile                    = SLOW

Post-promotion:

    controlled fixture          = 0
    controlled lazy rule        = 0
    stage                       = 0
    production source equality  = 4 / 4

Production HEALTH and topology gate:

    fixed mangle       = 8 / 8
    health fail-list   = 0
    AWG                = 1 / 1 / 1

Result:

    PASS

### Step 4 — FAST -> MIDDLE

Desired source:

    HEALTH  6150 / fe1c4593467d5283
    FAST    26100 / a6df6efc895000b4
    DETECT  41521 / c31920dc5302d7db
    JUDGE   16842 / 6654d7bbb164e505

Promotion result:

    SUCCESS

Observed cleanup scan:

    lazy rules:
        initial=1
        remaining=0
        verification attempts=1

    port-aware lists:
        initial=1
        remaining=0
        verification attempts=1

Rollback invariant:

    rollback backups                    = 4 / 4
    immediate previous backup equality = 4 / 4
    previous profile                    = FAST

Post-promotion:

    controlled fixture          = 0
    controlled lazy rule        = 0
    stage                       = 0
    production source equality  = 4 / 4

Production HEALTH and topology gate:

    fixed mangle       = 8 / 8
    health fail-list   = 0
    AWG                = 1 / 1 / 1

Result:

    PASS

### Bounded-sequence conclusion

Complete sequence:

    FAST -> MIDDLE -> SLOW -> FAST -> MIDDLE

Result:

    PASS

Accepted transitions:

    4 / 4

After every transition:

    exact desired production                  = PASS
    immediate previous rollback source        = PASS
    managed production scripts                = exactly 4
    managed schedulers                        = exactly 4
    managed schedulers enabled                = 4 / 4
    rollback backup objects                   = exactly 4
    controlled incompatible previous runtime  = 0
    stale stage objects                       = 0
    false HEALTH fail-list                    = 0
    fixed AUTO-AWG mangle                     = 8 / 8
    AWG infrastructure                        = unchanged

### Reference recovery

Managed schedulers were paused and jobs reached:

    jobs-idle=true

Exact stable v0.11.5 production was restored:

    HEALTH  4186
    FAST    4041
    DETECT  8075
    JUDGE   6122

Final M6 V2 gates:

    production sources       = 4186 / 4041 / 8075 / 6122
    schedulers               = 4 / 4
    fixed mangle             = 8 / 8
    AWG                      = 1 / 1 / 1
    accepted DEV3 stage      = 4
    DEV3 holds               = 0
    previous snapshots       = 0
    controlled fixture       = 0
    port-aware runtime       = 0
    lazy runtime             = 0
    health fail-list         = 0
    completed M6 steps       = 4 / 4
    rollback backups         = 0
    E2E safety copies        = 0
    temporary M6 V2 harness  = 0

Accepted DEV3 inert stage was restored exactly:

    HEALTH  5776
    FAST    26100
    DETECT  41521
    JUDGE   16842

### Controller end-state

A separate read-only post-test gate confirmed:

    stable v0.11.5 = RUNNING
    ca8f422        = STOPPED
    490eb83        = STOPPED

It also independently confirmed:

    production source       = 4186 / 4041 / 8075 / 6122
    AWG                     = 1 / 1 / 1
    accepted stage objects  = 4
    DEV3 holds              = 0
    rollback backups        = 0
    E2E safety copies       = 0
    M6 previous snapshots   = 0
    lazy rules              = 0
    health fail-list        = 0
    temporary harness       = 0

The follow-up read-only command was pasted as separate terminal commands, so
its `:local sched` and `:local fixed` variables did not persist between
commands.  Those two aggregate print lines are therefore not evidence either
way.  The authoritative M6 V2 harness had already verified:

    schedulers = 4 / 4
    fixed      = 8 / 8

before returning PASS.

### M6 conclusion

M6 is accepted.

The replacement DEV4 candidate survives repeated profile switching without
accumulating managed objects, stale stage state, incompatible controlled
runtime, or rollback generations.

Every rollback set represented the exact immediately previous production
source.

The RouterOS 7.23.3 HEALTH fix remained healthy across every repeated
transition and retained the original fail-open safety gate.

The executable candidate remains:

    490eb839cb42551707b6640bd20f7a5d29484f5b

Next migration acceptance case:

    M7 — live conntrack churn
