#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/reading-goal-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 \
  -I../reading-cloud/stubs -I../reading-suite/stubs -I../../lib/Memory -I../../lib/Serialization \
  test.cpp ../../src/apps_local/reading/ReadingGoalCache.cpp \
  ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/reading/ReadingStatsStore.cpp \
  ../../src/apps_local/reading/ReadingStatsProjection.cpp ../../src/apps_local/reading/ReadingStatsCloudStore.cpp \
  -o "$BUILD_DIR/test"
"$BUILD_DIR/test"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-parentheses -O2 \
  -I../xtc-reading/stubs -I../reading-cloud/stubs -I../reading-suite/stubs \
  -I../../lib/Memory -I../../lib/Serialization -I../../lib/Xtc -I../../lib/GfxRenderer \
  cover.cpp ../../lib/Xtc/Xtc.cpp ../../lib/Xtc/Xtc/XtcParser.cpp ../../lib/GfxRenderer/BitmapHelpers.cpp \
  -o "$BUILD_DIR/cover"
"$BUILD_DIR/cover"
