#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
# CI supplies the Classic program from its single all-environment invocation.
# Local host-test discovery runs before firmware builds, so build it here.
if [ -z "${CLASSIC_NAV_PROGRAM:-}" ]; then
  PIO="${PLATFORMIO:-pio}"
  if ! command -v "$PIO" >/dev/null 2>&1 && [ -x "$HOME/.platformio/penv/bin/pio" ]; then
    PIO="$HOME/.platformio/penv/bin/pio"
  fi
  "$PIO" run -e simulator_x4_classic
  CLASSIC_NAV_PROGRAM="$ROOT/.pio/build/simulator_x4_classic/program"
fi
test -x "$CLASSIC_NAV_PROGRAM"
if [ "$(uname -s)" = Linux ]; then
  export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
  export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
fi
OUT="${CLASSIC_NAV_OUTPUT:-$(mktemp -d "${TMPDIR:-/tmp}/classic-navigation-results.XXXXXX")}"
echo "Classic navigation artifacts: $OUT"
args=(--program "$CLASSIC_NAV_PROGRAM" --output "$OUT")
if [ -n "${CLASSIC_NAV_PRO_PROGRAM:-}" ]; then
  test -x "$CLASSIC_NAV_PRO_PROGRAM"
  args+=(--pro-program "$CLASSIC_NAV_PRO_PROGRAM")
fi
uv run --quiet --with pillow python host-tests/classic-navigation/test_runtime.py "${args[@]}"
