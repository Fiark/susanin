# Susanin v0.11.3 — pilot

First public pilot candidate.

## What is proven

- ARM64 / RouterOS 7.23.3 real-device validation;
- credentialless bootstrap;
- clean install from `0/16` managed objects to a running data plane;
- generated RouterOS script validation;
- `4/4` scripts, `4/4` schedulers, `8` managed mangle rules + safety rules;
- reboot recovery;
- fail-open DIRECT while the selected tunnel is unavailable;
- automatic return to adaptive routing after tunnel recovery;
- protocol-separated TCP/UDP learning;
- controller upgrade without stopping the RouterOS data plane;
- API reply framing fix for `!empty` followed by final `!done`.

## Pilot warning

This is not a mature appliance. Back up RouterOS before use. Current public focus is ARM64 and the tested baseline is RouterOS 7.23.3.

## Release assets

- `susanin.tar`
- `install.rsc`
- `uninstall.rsc`
- `uninstall-controller.rsc`
- `SHA256SUMS`
