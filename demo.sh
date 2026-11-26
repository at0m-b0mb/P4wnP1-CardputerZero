#!/usr/bin/env bash
# Run the panel against a stand-in service, so there is something to look at.
#
# live.sh watches and rebuilds, but it starts no service, so the panel opens
# on "No service" -- correct, and not a demo. This starts tests/fake_service.py
# first, points the panel at it, and tears it down on exit.
#
#   ./demo.sh              the six screens, light theme
#   ./demo.sh --dark       dark theme
#   ./demo.sh --redact     as you would capture it for publishing
set -uo pipefail
cd "$(dirname "$0")/app" || exit 1
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig"

APP=./build/darwin-arm64/Debug/p4wnp1_panel
PORT=${P4WNP1_DEMO_PORT:-8099}
REDACT=0
PAGE=status

for a in "$@"; do
  case "$a" in
    --dark)   DARK=yes ;;
    --light)  DARK=no ;;
    --redact) REDACT=1 ;;
    --page=*) PAGE="${a#--page=}" ;;
    *) echo "unknown option: $a"; exit 2 ;;
  esac
done

if [ ! -x "$APP" ]; then
  echo "[demo] building..."
  cmake --preset darwin-arm64 >/dev/null 2>&1
  cmake --build --preset darwin-arm64-dbg >/dev/null 2>&1 || { echo "[demo] build failed"; exit 1; }
fi

[ -n "${DARK:-}" ] && sed -i '' "s/^dark_mode=.*/dark_mode=$DARK/" config/p4wnp1_panel.conf

TOKEN=$(mktemp)
printf 'test-token-not-a-real-credential' > "$TOKEN"

python3 tests/fake_service.py "$PORT" >/dev/null 2>&1 &
FAKE=$!
cleanup() { kill "$FAKE" 2>/dev/null; rm -f "$TOKEN"; }
trap cleanup EXIT INT TERM

for _ in $(seq 1 60); do
  curl -s -o /dev/null "http://127.0.0.1:$PORT/api/v1/rpc" && break
  sleep 0.1
done

cat <<'KEYS'
[demo] the panel is open. Keys:

    Tab            next page  (Status, Cable, Payloads, Loadouts, Jobs, System)
    Up / Down      move the cursor
    Left / Right   jump three rows            (Cable)
    Space          toggle a USB function      (Cable)
    Enter          act on the selection       (asks first)
    Y / N          answer the question
    X              stop everything running    (Payloads, Jobs)
    B              apply a loadout at boot    (Loadouts)
    6              light / dark
    7              help for this page
    Esc            dismiss, then go back

    close the window to stop
KEYS

env P4WNP1_API="http://127.0.0.1:$PORT" \
    P4WNP1_TOKEN="$TOKEN" \
    P4WNP1_PAGE="$PAGE" \
    P4WNP1_REDACT="$REDACT" \
    "$APP"
