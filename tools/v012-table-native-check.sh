#!/usr/bin/env bash
set -euo pipefail

echo "=== Susanin v0.12 table-native guard ==="

HEALTH="templates/health.rsc.tmpl"

PING_TABLE_COUNT="$(
    grep -Fc \
        'routing-table="{{ROUTING_TABLE}}"' \
        "$HEALTH" ||
        true
)"

echo "HEALTH table-bound probes=$PING_TABLE_COUNT"

if [[ "$PING_TABLE_COUNT" != "2" ]]; then
    echo "ERROR: HEALTH must have exactly two table-bound probes"
    exit 1
fi

if grep -Fq \
    '{{EGRESS_INTERFACE}}' \
    "$HEALTH"
then
    echo "ERROR: HEALTH still depends on EGRESS_INTERFACE"
    exit 1
fi

if grep -Fq \
    'src-address=$awgIP' \
    "$HEALTH"
then
    echo "ERROR: legacy interface-address HEALTH returned"
    exit 1
fi

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

echo "PASS: HEALTH is routing-table native"
echo "PASS: table target does not require one egress interface"
echo "PASS: table mode leaves NAT ownership to routing design"
echo "PASS: setup supports interface/table choice"
echo "PASS: setup enforces strict IPv4-only RouterOS mode"
echo "PASS: status reports target mode"
echo "PASS: FAST/SOFT/JUDGE remain untouched"
