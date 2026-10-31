#!/bin/bash
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

checks=0
failed=0
ok()   { checks=$((checks + 1)); }
bad()  { checks=$((checks + 1)); failed=$((failed + 1)); echo "FAIL release  $1"; }

TABLE="$ROOT/partitions.csv"
[ -f "$TABLE" ] || { echo "FAIL release  missing $TABLE"; exit 1; }

CHIP=$((0x1000000))  # 16MB, the flash on both boards (board_upload.flash_size)
app_sizes=""
prev_end=0
overlap=0
for row in $(grep -vE '^\s*#' "$TABLE" | grep -vE '^\s*$' | tr -d ' \t' ); do
  IFS=',' read -r name type sub off size _ <<< "$row"
  [ -n "${off:-}" ] && [ -n "${size:-}" ] || continue
  o=$((off)); z=$((size))
  # Regions are listed in ascending order; a row starting before the previous
  # one ended is an overlap, which esptool does not reject and the bootloader
  # discovers by loading garbage.
  [ "$o" -lt "$prev_end" ] && overlap=1
  prev_end=$((o + z))
  if [ "$type" = "app" ]; then
    app_sizes="$app_sizes $z"
    if [ $((o % 0x10000)) -eq 0 ]; then ok; else
      bad "app partition '$name' starts at $off, which is not 64KB-aligned; the S3 maps app flash in 64KB pages"
    fi
  fi
done

if [ "$overlap" -eq 0 ]; then ok; else
  bad "partitions.csv has overlapping regions"
fi

if [ "$prev_end" -le "$CHIP" ]; then ok; else
  bad "partitions.csv allocates $prev_end bytes on a $CHIP byte chip; the last region runs off the end"
fi

# Two app slots, both the same size. Unequal slots are worse than small ones:
# an image that fits the slot it was flashed into but not the other one gets
# OTA'd once and then can never be updated again from the slot it landed in.
set -- $app_sizes
if [ "$#" -eq 2 ]; then
  ok
  if [ "$1" -eq "$2" ]; then ok; else
    bad "the two app slots differ ($1 vs $2); an OTA can land an image that fits one and not the other"
  fi
else
  bad "expected exactly 2 app partitions (ota_0/ota_1), found $#; OTA needs a second slot to land in"
fi

echo "$checks checks, $failed failed"
[ "$failed" -eq 0 ]
