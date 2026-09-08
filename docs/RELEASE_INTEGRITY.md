# Release integrity

Susanin stable releases use frozen, field-tested artifacts.

The stable release process intentionally separates:

- the commit used to build the tested runtime artifact;
- later documentation/release-only commits;
- the final Git tag;
- the exact binary assets attached to GitHub Release.

This avoids replacing a field-tested RouterOS container with a new CI rebuild
that has never been tested on the reference router.

## v0.12.0 artifact source

The final v0.12.0 runtime artifact was built from:

~~~text
ARTIFACT_SOURCE_SHA=d53517dfd6daccb7073517661138d79c573cac46
~~~

This commit contains the accepted runtime changes, including:

- final VPN Direct routing semantics;
- graceful SIGTERM/SIGINT controller shutdown;
- the previously accepted v0.12.0 adaptive data plane;
- unchanged accepted runtime GC.

Runtime/build inputs are frozen from this commit.

Protected paths include:

~~~text
.dockerignore
Dockerfile
Makefile
src/
templates/
bootstrap/
~~~

Documentation and release-only commits may exist after
`ARTIFACT_SOURCE_SHA`, but the protected paths above must remain identical
between the artifact source and the stable release tag.

## v0.12.0 frozen `susanin.tar`

Exact field-tested artifact:

~~~text
filename:
susanin.tar

size:
4199936 bytes

SHA256:
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f
~~~

RouterOS image identity observed during field acceptance:

~~~text
tag:
0.12.0

platform:
linux/arm64

image-id:
1200eeb2800f0e876580b61b63f14a4686c5338f9ead9979bc312ca93dac2e82
~~~

Binary SHA256 from the accepted image:

~~~text
a9a4b963088682eb27c4ab50eae7bd7800949e0d1f98ec230a769830434b7d51
~~~

The stable GitHub release must use these exact `susanin.tar` bytes.

It must not replace this file with a later rebuild.

## Bootstrap assets

Exact accepted bootstrap files:

~~~text
192104fc8012958dc511f2f5e7db5ff58be9b7727a287fcfae5e2854407be1c0  install.rsc
6067daacdfb7a047aa4791d2d7d46796dc3a26102c6d9e1e1b1da2f5f40bec98  uninstall.rsc
c5b6751f7d0907cc5db2e904ee32832e92446adfff3e9f9b64d2343ba522d4d3  uninstall-controller.rsc
~~~

Accepted sizes:

~~~text
install.rsc                10586 bytes
uninstall.rsc               4265 bytes
uninstall-controller.rsc    1653 bytes
SHA256SUMS                    327 bytes
~~~

## Frozen release bundle

The accepted v0.12.0 release directory contains exactly five files:

~~~text
susanin.tar
install.rsc
uninstall.rsc
uninstall-controller.rsc
SHA256SUMS
~~~

The frozen local bundle used for field acceptance was:

~~~text
susanin-v0.12.0-release-d53517d
~~~

The bundle is treated as immutable.

Do not regenerate `susanin.tar` after field acceptance.

Do not replace the accepted bootstrap files with newly generated copies,
even if their source appears equivalent.

## SHA256SUMS

`SHA256SUMS` covers the four deployable release files:

~~~text
81f982953e3b4d75c343b7729685938d7fc21105ea2df2393ca49117e0aadb2f  susanin.tar
192104fc8012958dc511f2f5e7db5ff58be9b7727a287fcfae5e2854407be1c0  install.rsc
6067daacdfb7a047aa4791d2d7d46796dc3a26102c6d9e1e1b1da2f5f40bec98  uninstall.rsc
c5b6751f7d0907cc5db2e904ee32832e92446adfff3e9f9b64d2343ba522d4d3  uninstall-controller.rsc
~~~

Users should verify the downloaded files before deployment:

~~~bash
sha256sum -c SHA256SUMS
~~~

## Field acceptance

The exact frozen v0.12.0 artifact was tested on a real MikroTik reference
system:

~~~text
RouterOS:
7.23.3 stable

architecture:
ARM64

routing target:
r_to_awg

resolved egress:
wg-awg-proxy
~~~

Acceptance included:

- exact ARM64 image startup;
- runtime version `Susanin 0.12.0`;
- credentialless bootstrap;
- RouterOS API authentication;
- first-run setup;
- generated RouterOS source validation;
- structural reconciliation;
- port-aware adaptive data plane;
- VPN Direct IPv4 policy;
- VPN Direct domain policy;
- forced VPN egress for VPN Direct;
- VPN Direct ordering before adaptive rules;
- graceful SIGTERM shutdown;
- controller restart after graceful stop;
- preservation of RouterOS data plane while controller was stopped;
- full uninstall;
- restoration of RouterOS API state on uninstall;
- preservation of independent VPN/routing/NAT objects on uninstall;
- fresh reinstall from the same frozen assets.

Final structural reference:

~~~text
KEEP=16 CREATE=0 UPDATE=0 BLOCKERS=0
Result: IN SYNC structurally.
~~~

Fresh data-plane reference:

~~~text
scripts=4
schedulers=4
fixed mangle=8
safety bypass=3
~~~

## Independent infrastructure preservation

The acceptance router contained VPN infrastructure not owned by Susanin:

~~~text
wg-awg-proxy
r_to_awg
0.0.0.0/0 via wg-awg-proxy
AWG selected traffic masquerade
~~~

Full `uninstall.rsc` removed Susanin-owned objects and preserved all of these
independent objects.

This is an explicit release acceptance requirement.

## Stable tag versus artifact source

The stable tag may point to a commit later than
`ARTIFACT_SOURCE_SHA`.

For example, later commits may contain:

- README updates;
- release notes;
- tested-scenarios documentation;
- security documentation;
- release workflow safety changes.

They must not modify the frozen runtime/build inputs.

Therefore these two identities are intentionally different:

~~~text
ARTIFACT_SOURCE_SHA = commit used to build the field-tested runtime
RELEASE_TAG_SHA     = final release/documentation commit
~~~

The stable tag does not imply that `susanin.tar` was rebuilt from
`RELEASE_TAG_SHA`.

For v0.12.0, rebuilding from the release tag would violate the frozen-artifact
release procedure.

## GitHub Actions policy

The release workflow does not automatically run on stable tag pushes.

It does not:

- run `docker buildx build`;
- rebuild `susanin.tar`;
- create a GitHub Release;
- upload release assets;
- publish a release.

It is a manual verification workflow only.

The verifier checks:

1. the requested tag exists;
2. `ARTIFACT_SOURCE_SHA` exists;
3. the artifact source is an ancestor of the tag;
4. protected runtime/build inputs are unchanged between those commits;
5. source/bootstrap version plumbing matches the release version;
6. accepted bootstrap file SHA256 values match the repository;
7. the expected frozen v0.12.0 artifact identity is recorded.

The verifier deliberately does not rebuild `susanin.tar`.

## Release publication policy

For v0.12.0 the stable release must be created from the already accepted
frozen bundle.

Publication order:

1. finish documentation and release-only changes;
2. verify protected runtime/build paths still match `ARTIFACT_SOURCE_SHA`;
3. create the final stable tag;
4. run the manual frozen-release verifier;
5. create the GitHub Release;
6. upload exactly the five frozen bundle files;
7. verify uploaded asset names and checksums;
8. publish the release.

If an uploaded asset differs from the accepted frozen artifact, do not
publish it as v0.12.0.

## Why CI rebuilding is not used for v0.12.0

Container builds are not assumed to be byte-for-byte reproducible across
different build times and environments.

A newly rebuilt image may have:

- different image metadata;
- different base-image content;
- different layer serialization;
- different tar bytes;
- a different final SHA256;

even when the source tree appears unchanged.

The artifact that passed field acceptance is therefore the release artifact.

## User verification

After downloading the stable release:

~~~bash
sha256sum -c SHA256SUMS
~~~

Expected result:

~~~text
susanin.tar: OK
install.rsc: OK
uninstall.rsc: OK
uninstall-controller.rsc: OK
~~~

Do not deploy files that fail this verification.

## Historical releases

Previous releases used earlier release procedures.

Their historical checksums and release notes remain available in Git history
and in their corresponding GitHub Releases.

The frozen-artifact procedure documented here is the authoritative process
for stable v0.12.0.
