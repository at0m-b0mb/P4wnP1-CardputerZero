#!/bin/bash
# Install the panel into a clean Debian arm64 container, check what landed,
# remove it, and check what is left behind. This is the "no SD swapping"
# claim: it has to go on and come off an ordinary system.
set -euo pipefail
cd "$(dirname "$0")"
DEB=$(ls app/dist/*.deb | head -1)
docker run --rm -v "$PWD/app/dist:/pkg:ro" debian:trixie-slim bash -eu -c "
  apt-get update -qq >/dev/null

  echo '--- before ---'
  test -e /usr/bin/p4wnp1_panel && echo '  UNEXPECTED: binary already present' || echo '  clean system'

  echo '--- install ---'
  apt-get install -y -qq /pkg/$(basename "$DEB") >/dev/null 2>&1
  dpkg -s p4wnp1-panel | grep -E '^(Package|Status|Version|Architecture):' | sed 's/^/  /'

  echo '--- what landed ---'
  for f in /usr/bin/p4wnp1_panel \
           /etc/p4wnp1_panel.conf \
           /usr/share/APPLaunch/applications/p4wnp1_panel.desktop \
           /usr/share/APPLaunch/share/images/p4wnp1_panel.png \
           /usr/lib/systemd/system/p4wnp1_panel.service; do
    test -e \"\$f\" && echo \"  present  \$f\" || echo \"  MISSING  \$f\"
  done

  echo '--- is the binary the right machine? ---'
  head -c 20 /usr/bin/p4wnp1_panel | od -An -tx1 | head -1 | sed 's/^/  ELF header:/'

  echo '--- the launcher entry ---'
  sed 's/^/  /' /usr/share/APPLaunch/applications/p4wnp1_panel.desktop

  echo '--- conffile registered? ---'
  grep p4wnp1 /var/lib/dpkg/info/p4wnp1-panel.conffiles 2>/dev/null | sed 's/^/  /' || echo '  none'

  echo '--- remove ---'
  apt-get remove -y -qq p4wnp1-panel >/dev/null 2>&1
  echo '--- what is left ---'
  left=0
  for f in /usr/bin/p4wnp1_panel \
           /usr/share/APPLaunch/applications/p4wnp1_panel.desktop \
           /usr/share/APPLaunch/share/images/p4wnp1_panel.png \
           /usr/lib/systemd/system/p4wnp1_panel.service; do
    if test -e \"\$f\"; then echo \"  LEFT BEHIND  \$f\"; left=1; fi
  done
  test \$left -eq 0 && echo '  nothing left behind (conffile is kept by design on remove)'
  test -e /etc/p4wnp1_panel.conf && echo '  /etc/p4wnp1_panel.conf kept -- expected, purge removes it'

  echo '--- purge ---'
  apt-get purge -y -qq p4wnp1-panel >/dev/null 2>&1
  test -e /etc/p4wnp1_panel.conf && echo '  conffile STILL present after purge' || echo '  conffile removed by purge'
"
