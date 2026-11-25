#!/bin/bash
# Capture one screen. Fails loudly if the app did not actually produce a new
# frame -- copying "the newest png in the folder" silently yields the previous
# capture when the run dies, which is how a stale image gets reported as new.
set -uo pipefail
cd "$(dirname "$0")"
out="$1"; page="${2:-status}"; dark="${3:-no}"; state="${4:-0}"
shots="$HOME/Pictures/Screenshots"
mkdir -p "$shots"

before=$(ls -t "$shots"/*.png 2>/dev/null | sed -n 1p || true)
sed -i '' "s/^dark_mode=.*/dark_mode=$dark/" config/p4wnp1_panel.conf
env P4WNP1_PAGE="$page" P4WNP1_DEPLOY_STATE="$state" P4WNP1_SHOT=1100 \
    ${P4WNP1_API:+P4WNP1_API="$P4WNP1_API"} ${P4WNP1_TOKEN:+P4WNP1_TOKEN="$P4WNP1_TOKEN"} \
    timeout 25 ./build/darwin-arm64/Debug/p4wnp1_panel >/dev/null 2>&1
after=$(ls -t "$shots"/*.png 2>/dev/null | sed -n 1p || true)

if [ -z "$after" ] || [ "$after" = "$before" ]; then
    echo "  FAILED: $out -- the app produced no new frame" >&2
    exit 1
fi
cp "$after" "screenshot/$out.png"
echo "  captured $out  (page=$page dark=$dark state=$state)"
