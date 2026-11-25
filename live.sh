#!/bin/bash
# Watch the P4wnP1 sources, rebuild, and relaunch the simulator window.
# Stop it with:  pkill -f 'p4wnp1_panel/live.sh'
cd "$(dirname "$0")/app" || exit 1
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig"
APP=./build/darwin-arm64/Debug/p4wnp1_panel
LOG=/tmp/p4wnp1_panel-build.log

sig() {
  find src assets config CMakeLists.txt tests 2>/dev/null \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.conf' -o -name '*.txt' \) \
    -exec stat -f '%m %N' {} + 2>/dev/null | sort | md5 -q
}

pid=""; last=""
printf '[live] watching. the window reloads on every save.\n'
while true; do
  s=$(sig)
  if [ "$s" != "$last" ]; then
    last=$s
    if cmake --build --preset darwin-arm64-dbg >"$LOG" 2>&1; then
      [ -n "$pid" ] && kill "$pid" 2>/dev/null
      "$APP" >/dev/null 2>&1 &
      pid=$!
      printf '[live] %s  rebuilt, window relaunched\n' "$(date +%H:%M:%S)"
    else
      printf '[live] %s  BUILD FAILED (window left as it was)\n' "$(date +%H:%M:%S)"
      grep -E 'error:' "$LOG" | head -4 | sed 's/^/        /'
    fi
  fi
  sleep 1
done
