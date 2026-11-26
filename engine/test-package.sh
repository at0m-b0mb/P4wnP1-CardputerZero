#!/usr/bin/env bash
#
# test-package.sh -- install, upgrade, remove and purge the engine package with
#                    a real dpkg, and assert what it did to the machine.
#
#   ./engine/test-package.sh
#
# WHY A CONTAINER AND NOT A DRY RUN
#
# The claims this package makes are all about SIDE EFFECTS -- a symlink exists,
# credentials are 0600, the operator's tree survives a remove, NetworkManager
# was left alone. A dry run proves none of those. So this runs real dpkg
# against a real Debian trixie arm64 filesystem and then looks at it.
#
# It is not the device. It cannot be: there is no CM0 here, dwc2 has nothing to
# bind to and systemd is not running as pid 1 inside the container. What it DOES
# cover is every line of the maintainer scripts that touches the filesystem,
# which is where a packaging bug eats somebody's work.
#
# THE TEST THAT MATTERS MOST
#
# "NetworkManager was not disabled" is the whole reason this package exists
# instead of install.sh. It is asserted here by installing NetworkManager
# first, taking the enablement state before and after, and comparing -- not by
# grepping the postinst for the absence of a command, which would pass just as
# happily if the postinst were empty.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB="$(ls -t "$HERE"/dist/p4wnp1-aloa_*_arm64.deb 2>/dev/null | head -1)"
[ -n "$DEB" ] || { echo "no .deb in engine/dist -- run build-engine-deb.sh first" >&2; exit 2; }

echo "testing $(basename "$DEB")"

docker run --pull=never -i --platform linux/arm64 \
    -v "$HERE/dist:/deb:ro" debian:trixie-slim bash -s <<'SCRIPT'
set -uo pipefail
PASS=0; FAIL=0; SKIP=0
ok()   { PASS=$((PASS+1)); printf '  \033[1;32mPASS\033[0m  %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); printf '  \033[1;31mFAIL\033[0m  %s\n' "$1"; }
# A skipped check is printed, counted and named. An invisible skip is how a
# suite reports 27 passes while the one assertion that mattered never ran.
skip() { SKIP=$((SKIP+1)); printf '  \033[1;33mSKIP\033[0m  %s (%s)\n' "$1" "$2"; }
check(){ if eval "$2" >/dev/null 2>&1; then ok "$1"; else bad "$1"; fi; }
no()   { if eval "$2" >/dev/null 2>&1; then bad "$1"; else ok "$1"; fi; }

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1

DEB=$(ls -t /deb/p4wnp1-aloa_*_arm64.deb | head -1)

# --- the NetworkManager control -------------------------------------------
#
# Install it so there is something to leave alone, then record its enablement
# BEFORE our package goes anywhere near the machine.
#
# The ordering here is load-bearing and the first version of this test got it
# wrong: it read the "before" state before network-manager (and therefore
# systemctl) existed, captured the string "systemctl: command not found", and
# then compared that against "enabled" afterwards -- reporting that the
# installer had seized networking when it had not touched it. The check was
# measuring the container, not the package. Written the other way round it
# would have PASSED just as wrongly.
# systemd is named explicitly. Installing network-manager with
# --no-install-recommends does NOT pull it in, so the first version of this
# test had no systemctl, skipped the check, and reported a clean run with the
# one assertion that matters never executed.
NM_STATE=unavailable
apt-get install -y -qq --no-install-recommends systemd network-manager >/dev/null 2>&1 || true
if command -v systemctl >/dev/null 2>&1 && [ -f /usr/lib/systemd/system/NetworkManager.service ]; then
    systemctl enable NetworkManager >/dev/null 2>&1 || true
    NM_BEFORE=$(systemctl is-enabled NetworkManager 2>/dev/null || echo unknown)
    # Only meaningful if we actually got it into a known state to begin with.
    case "$NM_BEFORE" in
        enabled|disabled) NM_STATE=ready ;;
        *)                NM_STATE=indeterminate ;;
    esac
    echo "  (NetworkManager before install: $NM_BEFORE)"
fi

echo
echo "== install =="
apt-get install -y -qq "$DEB" >/tmp/install.log 2>&1 || { echo "INSTALL FAILED"; tail -30 /tmp/install.log; exit 1; }

check "binaries installed"                 '[ -x /usr/local/bin/P4wnP1_service ] && [ -x /usr/local/bin/P4wnP1_cli ]'
check "updater shipped too"                '[ -x /usr/local/bin/p4wnp1-update ]'
check "pristine payload present"           '[ -d /usr/share/p4wnp1-aloa/payload/keymaps ]'
check "live tree seeded"                   '[ -d /var/lib/p4wnp1/keymaps ] && [ -d /var/lib/p4wnp1/HIDScripts ]'
check "/usr/local/P4wnP1 is a symlink"     '[ -L /usr/local/P4wnP1 ]'
check "symlink points at /var/lib/p4wnp1"  '[ "$(readlink /usr/local/P4wnP1)" = /var/lib/p4wnp1 ]'
check "hardcoded PATH_ROOT resolves"       '[ -d /usr/local/P4wnP1/keymaps ]'
check "ums/ exists for the operator"       '[ -d /var/lib/p4wnp1/ums ]'
check "unit installed"                     '[ -f /usr/lib/systemd/system/P4wnP1.service ]'
check "credentials generated"              '[ -f /etc/p4wnp1/initial.conf ]'
check "credentials are 0600"               '[ "$(stat -c %a /etc/p4wnp1/initial.conf)" = 600 ]'
check "credentials carry a PSK"            'grep -q "P4WNP1_INITIAL_PSK=" /etc/p4wnp1/initial.conf'
check "VERSION recorded"                   '[ -s /var/lib/p4wnp1/VERSION ]'

# The shared-default mistake, asserted rather than assumed: the PSK must differ
# from anything shipped, so it cannot have come out of the package.
no    "PSK is not a fixed shipped string"  'grep -rq "$(sed -n "s/.*P4WNP1_INITIAL_PSK=.\(.*\).$/\1/p" /etc/p4wnp1/initial.conf)" /usr/share/p4wnp1-aloa/payload/'

case "$NM_STATE" in
  ready)
    NM_AFTER=$(systemctl is-enabled NetworkManager 2>/dev/null || echo unknown)
    if [ "$NM_BEFORE" = "$NM_AFTER" ]; then
        ok "NetworkManager enablement UNCHANGED by install (still $NM_AFTER)"
    else
        bad "NetworkManager went '$NM_BEFORE' -> '$NM_AFTER': the installer seized networking"
    fi
    # And the positive control, so the check above is known to be capable of
    # firing: disabling it by hand must be visible to the same measurement.
    systemctl disable NetworkManager >/dev/null 2>&1 || true
    NM_PROOF=$(systemctl is-enabled NetworkManager 2>/dev/null || echo unknown)
    if [ "$NM_PROOF" != "$NM_AFTER" ]; then
        ok "the measurement can detect a disable (saw '$NM_PROOF')"
    else
        bad "disabling NetworkManager did not change the reading -- this check cannot fail"
    fi
    systemctl enable NetworkManager >/dev/null 2>&1 || true
    ;;
  *)
    skip "NetworkManager left alone by install" "no usable systemctl/NM in this image: $NM_STATE"
    ;;
esac

echo
echo "== operator writes their own work, then upgrades =="
echo 'return "mine";' > /var/lib/p4wnp1/HIDScripts/my-payload.js
mkdir -p /var/lib/p4wnp1/store && echo 'my templates' > /var/lib/p4wnp1/store/precious.db
echo 'operator edit' >> /etc/p4wnp1/initial.conf
apt-get install -y -qq --reinstall "$DEB" >/tmp/up.log 2>&1 || { echo "REINSTALL FAILED"; tail -20 /tmp/up.log; }

check "operator payload survived upgrade"  '[ -f /var/lib/p4wnp1/HIDScripts/my-payload.js ]'
check "operator store/ untouched"          'grep -q "my templates" /var/lib/p4wnp1/store/precious.db'
check "edited credentials not clobbered"   'grep -q "operator edit" /etc/p4wnp1/initial.conf'
check "shipped payloads still refreshed"   '[ -f /var/lib/p4wnp1/HIDScripts/mac-01-hello.js ]'

echo
echo "== remove: the operator's work must stay =="
apt-get remove -y -qq p4wnp1-aloa >/tmp/rm.log 2>&1 || { echo "REMOVE FAILED"; tail -20 /tmp/rm.log; }

no    "binary gone"                        '[ -e /usr/local/bin/P4wnP1_service ]'
no    "symlink gone"                       '[ -e /usr/local/P4wnP1 ]'
check "LIVE TREE SURVIVED remove"          '[ -d /var/lib/p4wnp1 ]'
check "operator payload survived remove"   '[ -f /var/lib/p4wnp1/HIDScripts/my-payload.js ]'
check "operator store/ survived remove"    'grep -q "my templates" /var/lib/p4wnp1/store/precious.db'

echo
echo "== purge: now it goes =="
apt-get purge -y -qq p4wnp1-aloa >/tmp/pg.log 2>&1 || { echo "PURGE FAILED"; tail -20 /tmp/pg.log; }

no    "live tree removed by purge"         '[ -d /var/lib/p4wnp1 ]'
no    "credentials removed by purge"       '[ -f /etc/p4wnp1/initial.conf ]'
check "root filesystem still intact"       '[ -d /usr ] && [ -d /etc ] && [ -d /var ]'

echo
echo "---- $PASS passed, $FAIL failed, $SKIP skipped ----"
[ "$FAIL" -eq 0 ]
SCRIPT
