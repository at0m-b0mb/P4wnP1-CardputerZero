#!/bin/bash
# Every host-runnable suite. None of these needs a device.
set -uo pipefail
cd "$(dirname "$0")/app"
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
JSON="$OUT/include"; mkdir -p "$JSON/nlohmann"
curl -fsSL --max-time 90 \
  https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
  -o "$JSON/nlohmann/json.hpp"

fail=0
run() { printf '\n=== %s ===\n' "$1"; shift; "$@" || fail=1; }

clang++ -std=c++17 -Wall -Wextra tests/test_contrast.cpp -o "$OUT/contrast"
run "theme contrast"  "$OUT/contrast"

clang++ -std=c++17 -Wall -Wextra tests/test_gadget_budget.cpp -o "$OUT/budget"
run "endpoint budget" "$OUT/budget"

clang++ -std=c++17 -Wall -Wextra \
  src/service/network_custody.cpp tests/test_network_custody.cpp -o "$OUT/custody"
run "network custody" "$OUT/custody"

clang++ -std=c++17 -Wall -Wextra -I"$JSON" \
  src/service/status_parse.cpp tests/test_status_parse.cpp -o "$OUT/parse"
run "status parsing"  "$OUT/parse"

clang++ -std=c++17 -Wall -Wextra -I"$JSON" \
  src/service/status_parse.cpp src/service/rpc_client.cpp tests/test_rpc_integration.cpp \
  -lcurl -o "$OUT/rpc"
printf 'test-token-not-a-real-credential' > "$OUT/good.token"
printf 'wrong-token' > "$OUT/bad.token"
python3 tests/fake_service.py 8099 >/dev/null 2>&1 & FAKE=$!
for _ in $(seq 1 40); do curl -s -o /dev/null http://127.0.0.1:8099/api/v1/rpc && break; done
run "rpc against the fake service" "$OUT/rpc" http://127.0.0.1:8099 "$OUT/good.token" "$OUT/bad.token"
kill $FAKE 2>/dev/null

printf '\n%s\n' "$([ $fail -eq 0 ] && echo 'ALL SUITES PASS' || echo 'SUITE FAILURES')"
exit $fail
