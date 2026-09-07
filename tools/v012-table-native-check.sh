#!/usr/bin/env bash
set -euo pipefail

echo "=== Susanin v0.12 table-native guard ==="

HEALTH="templates/health.rsc.tmpl"

PING_TABLE_COUNT="$(
    grep -Fc \
        'routing-table=' \
        "$HEALTH" ||
        true
)"

HEALTH_INTERFACE_PROBES="$(
    grep -Fc \
        'interface="{{EGRESS_INTERFACE}}" src-address=$awgIP count=1' \
        "$HEALTH" ||
        true
)"

echo "HEALTH routing-table probes=$PING_TABLE_COUNT"
echo "HEALTH interface/src-address probes=$HEALTH_INTERFACE_PROBES"

if [[ "$PING_TABLE_COUNT" != "0" ]]; then
    echo "ERROR: RouterOS 7.23.3 HEALTH must not use /ping routing-table="
    exit 1
fi

if [[ "$HEALTH_INTERFACE_PROBES" != "2" ]]; then
    echo "ERROR: HEALTH must have exactly two interface/src-address probes"
    exit 1
fi

grep -Fq \
    ':local awgAddrIds [/ip address find where interface="{{EGRESS_INTERFACE}}"]' \
    "$HEALTH"

grep -Fq \
    'cfg->target_mode == SUSANIN_TARGET_ROUTING_TABLE' \
    src/config.c

grep -Fq \
    '<table-native / not required>' \
    src/target.c

grep -Fq \
    'NAT unmanaged for routing-table target' \
    src/install.c

grep -Fq \
    'Choose routing target type:' \
    src/setup.c

grep -Fq \
    '"/ipv6/settings/set"' \
    src/setup.c

grep -Fq \
    '"=disable-ipv6=yes"' \
    src/setup.c

grep -Fq \
    'Egress: <table-native / not required>' \
    src/status.c

if git diff --name-only HEAD -- \
    templates/fast.rsc.tmpl \
    templates/soft.rsc.tmpl \
    templates/judge.rsc.tmpl |
    grep -q .
then
    echo "ERROR: FAST/SOFT/JUDGE changed"
    exit 1
fi

echo "PASS: HEALTH uses RouterOS 7.23.3 compatible interface/src-address probes"
echo "PASS: adaptive routing remains table-native; HEALTH resolves concrete egress for probing"
echo "PASS: table mode leaves NAT ownership to routing design"
echo "PASS: setup supports interface/table choice"
echo "PASS: setup enforces strict IPv4-only RouterOS mode"
echo "PASS: status reports target mode"
echo "PASS: FAST/SOFT/JUDGE remain untouched"
