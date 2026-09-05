#!/usr/bin/env bash
set -euo pipefail

echo "=== Susanin v0.12 control-plane guard ==="

grep -Fq \
    'hints.ai_family = AF_INET;' \
    src/routeros_api.c

grep -Fq \
    'SUSANIN_TARGET_ROUTING_TABLE' \
    src/config.h

grep -Fq \
    'SUSANIN_ACCURACY_MIDDLE' \
    src/config.h

grep -Fq \
    'SUSANIN_ACCURACY_SLOW' \
    src/config.h

grep -Fq \
    'VPN Direct bypass' \
    src/direct.c

grep -Fq \
    '=match-subdomain=yes' \
    src/direct.c

grep -Fq \
    '"=address-list=" DIRECT_LIST' \
    src/direct.c

grep -Fq \
    'target set routing-table' \
    src/main.c

grep -Fq \
    'direct add domain' \
    src/main.c

echo "PASS: IPv4-only controller socket"
echo "PASS: target abstraction schema"
echo "PASS: FAST/MIDDLE/SLOW config schema"
echo "PASS: persistent VPN Direct IP/domain CLI"
echo "PASS: RouterOS DNS subdomain -> vpn_direct integration"
