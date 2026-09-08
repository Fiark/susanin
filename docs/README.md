# Susanin documentation

Documentation for stable Susanin v0.12.0.

> Start with the User Guide for installation and normal operation.
>
> Files under **Development archive** are historical engineering records.
> They are not the authoritative description of stable v0.12.0 behavior.

## Stable v0.12.0 documentation

### User guides

- [Русское руководство](USER_GUIDE.md)
- [English User Guide](USER_GUIDE_en.md)

These are the main installation, configuration, operation, troubleshooting and
uninstall guides.

### Architecture

- [Architecture](ARCHITECTURE.md)

Covers:

- control plane and RouterOS data plane separation;
- Interface and Routing table targets;
- VPN Direct;
- port-aware adaptive identity;
- lazy per-port rules;
- accuracy profiles;
- migration;
- runtime GC;
- fail-open behavior;
- ownership boundaries;
- IPv4-only scope.

### Upgrade

- [Обновление](UPGRADE.md)
- [Upgrading](UPGRADE_en.md)

Use these procedures when moving an existing installation to v0.12.0 or when
the desired RouterOS data plane changes.

### Logging and diagnostics

- [Логирование и диагностика](LOGGING.md)
- [Logging and Diagnostics](LOGGING_en.md)

Use these before opening a runtime bug report.

### Tested behavior

- [TESTED.md](TESTED.md)

Contains the stable v0.12.0 field-acceptance record, including:

- ARM64 / RouterOS 7.23.3;
- controller bootstrap and authentication;
- structural synchronization;
- Routing table target;
- VPN Direct IPv4 and domain behavior;
- graceful shutdown;
- uninstall;
- fresh reinstall;
- preservation of independent VPN infrastructure;
- runtime GC identity.

### Release integrity

- [RELEASE_INTEGRITY.md](RELEASE_INTEGRITY.md)

Defines the frozen stable artifact policy and exact v0.12.0 artifact identity.

### Stable release notes

- [v0.12.0](RELEASE_NOTES_v0.12.0.md)

## Stable v0.12.0 scope

Stable v0.12.0 includes:

- Interface targets;
- Routing table targets;
- VPN Direct;
- IPv4/CIDR/domain explicit policy;
- port-aware adaptive state;
- lazy per-port mangle rules;
- `fast`, `middle` and `slow` profiles;
- migration hardening;
- bounded runtime GC;
- fail-open behavior;
- graceful controller shutdown;
- staged promotion and rollback;
- IPv4-only operation.

Stable v0.12.0 does **not** include:

- IPv6 adaptive routing;
- TLS SNI inspection;
- SNI-assisted adaptive verification;
- SNI-based VPN Direct matching.

If an older development document conflicts with this stable scope, the stable
documentation and field-acceptance records take precedence.

## Previous stable release notes

Historical release notes:

- [v0.11.5](RELEASE_NOTES_v0.11.5.md)
- [v0.11.4](RELEASE_NOTES_v0.11.4.md)
- [v0.11.3](RELEASE_NOTES_v0.11.3.md)

They describe those releases only.

## Development archive

The following files are retained as engineering history for the v0.12
development cycle:

- [Historical v0.12 roadmap](ROADMAP_v0.12.md)
- [DEV2 port-aware work](DEV2_PORT_AWARE.md)
- [DEV3 accuracy profiles](DEV3_PROFILES.md)
- [DEV4 plan](DEV4_PLAN.md)
- [DEV4 migration](DEV4_MIGRATION.md)
- [DEV1 acceptance](ACCEPTANCE_v0.12.0-dev1.md)
- [DEV3 acceptance](ACCEPTANCE_v0.12.0-dev3.md)
- [DEV4 acceptance](ACCEPTANCE_v0.12.0-dev4.md)

These documents may contain:

- superseded implementation ideas;
- development-only terminology;
- previous v0.11.5 baselines;
- experimental plans that were later changed or removed.

In particular, historical SNI-related planning does **not** mean that SNI
verification shipped in stable v0.12.0.

## Background article

- [Habr article draft](habr-article.md)

This is explanatory/background material and is not a substitute for the stable
User Guide, Architecture, Tested or Release Integrity documents.

## Repository-level documents

From the repository root:

- [README](../README.md)
- [English README](../README_en.md)
- [Security](../SECURITY.md)
- [Changelog](../CHANGELOG.md)
- [Contributing](../CONTRIBUTING.md)

## Documentation precedence

When documents disagree, use this order:

1. stable v0.12.0 release notes and release integrity;
2. stable User Guide and Architecture;
3. stable Tested record;
4. upgrade and diagnostics guides;
5. previous release notes;
6. development archive and background material.
