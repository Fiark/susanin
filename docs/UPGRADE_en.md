# Upgrading Susanin to v0.12.0

Susanin has two independent layers:

- the container is the control plane;
- RouterOS `auto-awg-*` scripts and schedulers are the data plane.

Replacing the container does **not** mean that the existing RouterOS data
plane was automatically upgraded.

Use this sequence:

~~~text
backup
  |
  v
upgrade controller
  |
  v
verify target/config
  |
  v
validate + apply --dry-run
  |
  v
stage
  |
  v
promote --dry-run
  |
  v
promote
  |
  v
post-upgrade verification
~~~

Do not manually replace production RouterOS script source.

## 1. Back up RouterOS

Create a tested backup/recovery point before the upgrade.

Do not publish RouterOS backups or sensitive exports in GitHub Issues.

## 2. Download stable v0.12.0

Required:

~~~text
susanin.tar
install.rsc
SHA256SUMS
~~~

Recommended:

~~~text
uninstall.rsc
uninstall-controller.rsc
~~~

Verify the release:

~~~bash
sha256sum -c SHA256SUMS
~~~

Stable v0.12.0 uses frozen field-tested artifacts.

See:

[RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

## 3. Upload the new release

Upload the new:

~~~text
susanin.tar
install.rsc
~~~

Keep the exact filenames.

Make sure an older `susanin.tar` is not still present under the release name.

## 4. Check the bootstrap parser

~~~routeros
/import file-name=install.rsc verbose=yes dry-run
~~~

There should be no syntax errors.

## 5. Upgrade the controller

~~~routeros
/import file-name=install.rsc verbose=yes
~~~

The bootstrap replaces the Susanin controller while the RouterOS data plane
remains a separate layer.

Wait until:

~~~routeros
/container print detail where name="susanin-controller"
~~~

shows the controller as RUNNING.

Stable v0.12.0 should report:

~~~text
tag="0.12.0"
arch="arm64"
root-dir=/susanin-controller-v0120
~~~

## 6. Verify version and API authentication

Version:

~~~text
susanin version
~~~

Expected:

~~~text
Susanin 0.12.0
~~~

Discovery:

~~~text
susanin discover
~~~

Expected:

~~~text
Authenticated.
~~~

## 7. Verify migrated configuration

Run:

~~~text
susanin config show
susanin target show
~~~

### Upgrade from v0.11.x

v0.12.0 can read the previous v0.11.x configuration format.

If the new `target_mode` and `target_value` fields are absent, Susanin derives
an Interface target from the previously stored `egress_interface`.

The effective mode remains:

~~~text
interface
~~~

If `accuracy_profile` is absent, the default is:

~~~text
fast
~~~

An upgrade therefore does not silently convert an existing installation to
Routing table mode.

## 8. Decide whether to keep or change the target

### Keep Interface mode

If the existing egress interface is still the desired design, leave the target
unchanged.

Check:

~~~text
susanin target show
susanin discover
~~~

### Move to Routing table mode

If an existing RouterOS policy-routing table should become authoritative:

~~~text
susanin target list
susanin target set routing-table <name>
~~~

Example:

~~~text
susanin target set routing-table r_to_awg
~~~

Then run:

~~~text
susanin target show
susanin discover
susanin direct sync
~~~

For a routing-table target Susanin does not automatically own external tunnel
NAT.


## 9. Validate generated RouterOS source

After upgrading the controller and verifying the target:

~~~text
susanin validate
~~~

Stable v0.12.0 reference:

~~~text
PASS=4 FAIL=0
Production scripts changed: NO
~~~

Do not promote if validation fails.

## 10. Compare desired and production state

~~~text
susanin apply --dry-run
~~~

If the result is:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

the production data plane already matches v0.12.0.

No promotion is required.

## 11. If UPDATE is reported

When upgrading from v0.11.x to v0.12.0, UPDATE is expected because the data
plane moves to the port-aware model.

Do not replace production script source manually.

Create an inert stage:

~~~text
susanin stage
~~~

Expected stage objects:

~~~text
susanin-stage-health
susanin-stage-fast
susanin-stage-detect
susanin-stage-judge
~~~

The existing production data plane continues running while these stage scripts
are created.

## 12. Check promotion safety gates

~~~text
susanin promote --dry-run
~~~

Continue only if:

~~~text
Safety gates: PASS
~~~

Dry-run does not modify production.

## 13. Run transactional promotion

~~~text
susanin promote
~~~

Promotion performs:

1. a snapshot of current production;
2. creation of `susanin-backup-*`;
3. pause of managed schedulers;
4. wait for active managed jobs to become idle;
5. replacement of the four production script sources;
6. fingerprint verification;
7. cleanup of incompatible adaptive runtime state;
8. restoration of scheduler states;
9. removal of stage objects.

Reference success:

~~~text
Promotion result: SUCCESS
Rollback backups retained: YES
Scheduler states restored: YES
~~~

## 14. Migration from v0.11.x to v0.12.0

v0.11.x used the previous IP-oriented adaptive model.

v0.12.0 uses:

~~~text
protocol + destination IPv4 + destination port
~~~

Old runtime state must therefore not remain active after source replacement.

A successful promotion clears incompatible state including:

- legacy IP-only entries;
- old port-state entries;
- stale lazy per-port rules;
- adaptive connection marks.

After the upgrade, a healthy status should report:

~~~text
legacy IP-only entries=0
Adaptive migration state: clean
~~~

## 15. Verify production after promotion

Run:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

Stable v0.12.0 reference fingerprints:

~~~text
auto-awg-health   bytes=6148   fnv1a64=cafdf828c49d2946
auto-awg-fast     bytes=26098  fnv1a64=0c9672d93a6a4e85
auto-awg-detect   bytes=41519  fnv1a64=0ee9c8e6708bc6e8
auto-awg-judge    bytes=16840  fnv1a64=72733543f4561160
~~~

Structural reference:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 16. Verify schedulers

Reference cadence:

~~~text
auto-awg-health   interval=3s
auto-awg-fast     interval=1s
auto-awg-detect   interval=2s
auto-awg-judge    interval=1s
~~~

RouterOS checks:

~~~routeros
/system scheduler print detail where name~"^auto-awg"
/system script job print
~~~

After a successful promotion, all four managed schedulers should return to
their previous enabled/disabled states.


## 17. Verify VPN Direct

After changing the routing target:

~~~text
susanin direct list
susanin direct sync
~~~

If the policy is empty:

~~~text
Policy is empty.
~~~

If policy entries exist, verify that they use the newly selected target.

For domain policy also verify that RouterOS can observe client DNS traffic.

## 18. If promotion fails

Susanin attempts to preserve a safe state and restore production source.

Manual rollback is available:

~~~text
susanin rollback
~~~

Rollback:

- pauses managed schedulers;
- waits for active managed jobs to become idle;
- clears adaptive runtime state;
- restores `susanin-backup-*`;
- verifies restored source;
- restores scheduler states.

After a successful rollback:

~~~text
susanin snapshot
susanin status
susanin apply --dry-run
~~~

## 19. Failed-safe state

If runtime cleanup or source restoration cannot complete safely, Susanin may
leave the managed schedulers PAUSED.

This is intentional fail-safe behavior.

Do not manually re-enable them before identifying the cause.

Collect:

~~~text
susanin status
susanin snapshot
susanin diag errors
susanin apply --dry-run
~~~

## 20. Post-upgrade verification

Run:

~~~text
susanin version
susanin discover
susanin target show
susanin config show
susanin direct list
susanin status
susanin apply --dry-run
~~~

Healthy stable v0.12.0 reference:

~~~text
Susanin 0.12.0

Authenticated.

KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

## 21. What not to do

Do not:

- manually paste new source into `auto-awg-*`;
- manually delete old adaptive runtime lists before promotion;
- remove independent VPN infrastructure;
- recreate an existing routing table without a reason;
- run `setup` only to upgrade an already valid configuration;
- run `promote` when `promote --dry-run` does not pass.

## 22. Moving to Routing table mode after upgrade

After the controller upgrade, the target can be changed independently:

~~~text
susanin target set routing-table <name>
~~~

Then run:

~~~text
susanin target show
susanin discover
susanin direct sync
susanin validate
susanin apply --dry-run
~~~

If UPDATE is reported:

~~~text
susanin stage
susanin promote --dry-run
susanin promote
~~~

Then verify the final state again.

## 23. Upgrade diagnostics

If the upgrade produces an unexpected result:

~~~text
susanin diag start
susanin diag sample
susanin diag errors
susanin status
susanin snapshot
susanin apply --dry-run
susanin diag stop
~~~

See:

[LOGGING_en.md](LOGGING_en.md)

## 24. Stable v0.12.0 reference

Final v0.12.0 acceptance included:

~~~text
FRESH_BOOTSTRAP=PASS
FRESH_SETUP=PASS
STRUCTURAL_SYNC=PASS
TABLE_NATIVE_TARGET=PASS
GRACEFUL_SIGTERM=PASS
RESTART_AFTER_SIGTERM=PASS
FULL_UNINSTALL=PASS
FRESH_REINSTALL=PASS
GC_IDENTICAL=PASS
~~~

More details:

[TESTED.md](TESTED.md)
