#!/bin/sh
set -eu
root=${1:-.}
fail=0
check() {
  label=$1; pattern=$2
  if grep -RInE --exclude='*.o' --exclude='susanin' --exclude='secret-scan.sh' "$pattern" "$root" >/tmp/susanin-secret-scan.$$ 2>/dev/null; then
    echo "FAIL: $label"
    cat /tmp/susanin-secret-scan.$$
    fail=1
  fi
  rm -f /tmp/susanin-secret-scan.$$
}
check "environment credential variables" '(AUTOAWG|SUSANIN)_ROUTER_(USER|PASSWORD)'
check "private key material" 'BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY|PRIVATE_KEY=|AWG_(CLIENT|SERVER)_PUB='
if find "$root" -type f \( -name 'config.env' -o -name '.env' -o -name '*.backup' -o -name '*show-sensitive*' \) | grep -q .; then
  echo "FAIL: sensitive artifact filename present"
  find "$root" -type f \( -name 'config.env' -o -name '.env' -o -name '*.backup' -o -name '*show-sensitive*' \)
  fail=1
fi
if [ "$fail" -ne 0 ]; then exit 1; fi
echo "PASS: no credential artifacts detected"
