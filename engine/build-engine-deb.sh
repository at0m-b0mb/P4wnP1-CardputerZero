#!/usr/bin/env bash
#
# build-engine-deb.sh -- package P4wnP1 A.L.O.A. itself as an installable .deb.
#
#   ./engine/build-engine-deb.sh --bundle ../../P4wnP1_aloa/image/out/P4wnP1-ALOA-v0.9.0-update-arm64.tar.xz
#   ./engine/build-engine-deb.sh --version v0.9.0        # fetch the release asset
#
# Output: engine/dist/p4wnp1-aloa_<version>_arm64.deb
#
# WHY THIS EXISTS, AND WHAT IT IS NOT
#
# `p4wnp1-panel` -- the other package in this repository -- is the FACE. It is
# 1.9MB: a binary, a unit, a .desktop and some icons. It does not contain
# P4wnP1, does not depend on it, and on a machine without it you get a panel
# talking to an engine that is not there.
#
# This is the engine. Together they are the "apt install your rig onto the
# stock OS" claim in PLAN.md; separately neither of them is.
#
# IT IS BUILT FROM THE RELEASED UPDATE BUNDLE, ON PURPOSE
#
# The bundle p4wnp1-update installs already contains exactly what a package
# needs -- bin/, units/, www/, keymaps/, scripts/, HIDScripts/, db/init.db,
# VERSION, ARCH -- and it is the artifact that has actually been exercised on
# hardware. Building the .deb from the same bytes means an apt install and an
# in-place update cannot drift apart, and there is no second definition of
# "what P4wnP1 consists of" to keep in sync.
#
# THE /usr/local DEVIATION, STATED RATHER THAN HIDDEN
#
# Debian policy says a package must not ship into /usr/local. This one does,
# and the reason is that fourteen shipped files hardcode those paths:
# `common/globals.go` sets `PATH_ROOT = "/usr/local/P4wnP1"`, both systemd
# units `ExecStart=/usr/local/bin/...`, and the healthcheck, firstboot and
# updater scripts all look there. Relocating would mean patching the engine on
# its way into a package -- changing code that has been tested on a device, at
# packaging time, where nothing would re-test it.
#
# So: the binaries go to /usr/local/bin, and the operator's live tree is
# /var/lib/p4wnp1 with /usr/local/P4wnP1 as a symlink to it. That keeps the
# WRITABLE state (store/, ums/, payloads they wrote) in the place Debian
# reserves for exactly that, and keeps it across upgrades and removals, while
# every hardcoded path still resolves.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Where the caller actually was. Arguments are resolved against THIS, not
# against $HERE -- a script that cd's first and then opens a relative path the
# user typed is reading a different file from the one they named.
INVOKED_FROM="$PWD"
cd "$HERE"

BUNDLE=""
VERSION=""
REPO="at0m-b0mb/P4wnP1_aloa"

while [ $# -gt 0 ]; do
    case "$1" in
        --bundle)  BUNDLE="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

say() { printf '\033[1;32m[engine-deb]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[engine-deb:FAILED]\033[0m %s\n' "$*" >&2; exit 1; }

WORK="$(mktemp -d)"
cleanup() { [ -n "${WORK:-}" ] && [ -d "$WORK" ] && chmod -R u+w "$WORK" 2>/dev/null; [ -n "${WORK:-}" ] && rm -rf -- "$WORK"; }
trap cleanup EXIT

# --- get the bundle -------------------------------------------------------

if [ -z "$BUNDLE" ]; then
    [ -n "$VERSION" ] || die "pass --bundle <file> or --version <tag>"
    NAME="P4wnP1-ALOA-${VERSION}-update-arm64.tar.xz"
    BUNDLE="$WORK/$NAME"
    say "fetching $NAME from the $VERSION release"
    curl -fsSL --max-time 900 -o "$BUNDLE" \
        "https://github.com/$REPO/releases/download/$VERSION/$NAME" \
        || die "could not download $NAME. v0.8.0 and earlier shipped NO update
bundle, so there is nothing to fetch for those tags -- build one with
tools/build-update-bundle.sh in the P4wnP1_aloa repo and pass --bundle."
    curl -fsSL --max-time 60 -o "$BUNDLE.sha256" \
        "https://github.com/$REPO/releases/download/$VERSION/$NAME.sha256" 2>/dev/null || true
fi

case "$BUNDLE" in
    /*) ;;                                  # already absolute
    *)  BUNDLE="$INVOKED_FROM/$BUNDLE" ;;   # as the caller meant it
esac

[ -r "$BUNDLE" ] || die "no such bundle: $BUNDLE"

# Check it if a checksum came with it. Unlike the updater's own download this
# is a build-time check on a developer machine, so a missing checksum is a
# warning rather than a refusal -- but a WRONG one is fatal either way.
if [ -r "$BUNDLE.sha256" ]; then
    EXPECT="$(awk '{print $1}' "$BUNDLE.sha256")"
    ACTUAL="$(shasum -a 256 "$BUNDLE" | awk '{print $1}')"
    [ "$EXPECT" = "$ACTUAL" ] || die "checksum mismatch on $BUNDLE"
    say "checksum ok"
else
    say "no .sha256 beside the bundle -- packaging it unverified"
fi

SRC="$WORK/x"
mkdir -p "$SRC"
tar xJf "$BUNDLE" -C "$SRC" || die "the bundle will not unpack"

[ -r "$SRC/VERSION" ] || die "the bundle has no VERSION file; it is not an update bundle"
[ -r "$SRC/ARCH" ]    || die "the bundle has no ARCH file; it is not an update bundle"

BVER="$(tr -d '\n' < "$SRC/VERSION")"
BARCH="$(tr -d '\n' < "$SRC/ARCH")"

# The CardputerZero is a CM0: aarch64. An armhf bundle would install and then
# fail to execute a single binary, which is a confusing way to find out.
[ "$BARCH" = "arm64" ] || die "this bundle is for $BARCH. The CardputerZero is a
Raspberry Pi CM0 (aarch64), so it needs the arm64 bundle."

# Debian versions may not start with a letter, so v0.9.0 becomes 0.9.0. A
# bundle from an untagged tree (v0.9.0-3-gabc1234) becomes 0.9.0-3-gabc1234,
# which dpkg accepts and which says plainly that it is not a release.
DEBVER="${BVER#v}"
say "packaging $BVER ($BARCH) as p4wnp1-aloa $DEBVER"

# --- lay out the package tree --------------------------------------------

ROOT="$WORK/pkg"
mkdir -p "$ROOT/DEBIAN" \
         "$ROOT/usr/local/bin" \
         "$ROOT/usr/share/p4wnp1-aloa/payload" \
         "$ROOT/usr/lib/systemd/system" \
         "$ROOT/usr/share/doc/p4wnp1-aloa"

# Binaries. p4wnp1-oled is deliberately NOT installed: it drives an SSD1306
# over SPI, and on this device the panel in the sibling package is the screen.
for b in P4wnP1_service P4wnP1_cli p4wnp1-hashpw; do
    [ -f "$SRC/bin/$b" ] || die "$b missing from the bundle"
    install -m 0755 "$SRC/bin/$b" "$ROOT/usr/local/bin/$b"
done
# The updater ships too, so a device installed from apt can still take an
# in-place update later without reflashing or reinstalling the package.
[ -f "$SRC/bin/p4wnp1-update" ] && install -m 0755 "$SRC/bin/p4wnp1-update" "$ROOT/usr/local/bin/p4wnp1-update"

# The PRISTINE payload. postinst seeds the live tree from this; it is not the
# live tree itself, which is the whole reason an upgrade can refresh shipped
# files without touching anything the operator wrote.
for d in keymaps scripts HIDScripts www db helper legacy; do
    [ -d "$SRC/$d" ] && cp -R "$SRC/$d" "$ROOT/usr/share/p4wnp1-aloa/payload/"
done
echo "$BVER" > "$ROOT/usr/share/p4wnp1-aloa/payload/VERSION"

# Units. P4wnP1.service is taken from the bundle unchanged -- its
# ExecStart=/usr/local/bin/P4wnP1_service is correct for this layout, which is
# the point of not relocating.
for u in P4wnP1.service p4wnp1-firstboot.service; do
    [ -f "$SRC/units/$u" ] && install -m 0644 "$SRC/units/$u" "$ROOT/usr/lib/systemd/system/$u"
done
[ -f "$ROOT/usr/lib/systemd/system/P4wnP1.service" ] || die "P4wnP1.service missing from the bundle"

install -m 0644 "$HERE/README.md" "$ROOT/usr/share/doc/p4wnp1-aloa/README.md"

for s in postinst prerm postrm; do
    install -m 0755 "$HERE/debian/$s" "$ROOT/DEBIAN/$s"
done

INSTALLED_KB="$(du -sk "$ROOT" | awk '{print $1}')"

cat > "$ROOT/DEBIAN/control" <<CONTROL
Package: p4wnp1-aloa
Version: $DEBVER
Architecture: arm64
Maintainer: at0m-b0mb <99875896+at0m-b0mb@users.noreply.github.com>
Section: utils
Priority: optional
Installed-Size: $INSTALLED_KB
Depends: hostapd, dnsmasq, iw, rfkill, wireless-tools, bluez, bluez-tools,
 openssl, usbutils, kmod, python3, ca-certificates
Recommends: haveged, wpasupplicant, dosfstools, nftables, openssh-server
Suggests: p4wnp1-panel
Description: P4wnP1 A.L.O.A. -- USB attack and network platform
 The engine: the P4wnP1 service, CLI, payload tree, keymaps, web console and
 systemd units, built from the same update bundle the in-place updater
 installs.
 .
 This package does NOT take the machine's networking away. The upstream
 installer disables NetworkManager at install time, which breaks the
 CardputerZero's own LaunchWizard and Settings pages. Here, nothing touches
 networking until a mode is armed, and it is handed back on disarm, on crash
 and on removal.
 .
 Install p4wnp1-panel alongside this for the 320x170 on-device front-end.
CONTROL

# There is deliberately NO conffiles entry for /etc/p4wnp1/initial.conf.
#
# It was declared one at first, and dpkg refused the build: "conffile
# '/etc/p4wnp1/initial.conf' does not appear in package" -- correctly, because
# the credentials are GENERATED per device by postinst, not shipped. A shipped
# credentials file is exactly the shared-default mistake this project already
# made once, so it must not exist in the package at all.
#
# The property a conffile would have given -- an operator's edited file
# surviving an upgrade -- is already guaranteed by postinst, which writes the
# file only when it is absent.

# --- build ----------------------------------------------------------------

mkdir -p "$HERE/dist"
OUT="$HERE/dist/p4wnp1-aloa_${DEBVER}_arm64.deb"

# Ownership: everything root:root. Building as a normal user on macOS would
# otherwise bake this account's uid into the package, so the build runs in a
# container where it is already root.
say "building $OUT"
if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb --root-owner-group --build "$ROOT" "$OUT" >/dev/null
else
    say "no dpkg-deb on this host; building in a container"
    docker run --pull=never -i --user 0:0 \
        -v "$WORK:/w" -v "$HERE/dist:/out" debian:trixie-slim \
        dpkg-deb --root-owner-group --build /w/pkg "/out/$(basename "$OUT")" >/dev/null \
        || die "container build failed"
fi

( cd "$HERE/dist" && shasum -a 256 "$(basename "$OUT")" > "$(basename "$OUT").sha256" )

say "$(wc -c < "$OUT" | tr -d ' ') bytes"
say "$(awk '{print substr($1,1,16)}' "$OUT.sha256")"
say "done"
