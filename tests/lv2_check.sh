#!/bin/sh
# Plays the plugin the build just made, at three volume settings, and checks
# the level follows the control rather than merely making a noise.
set -e
BUNDLE="${1:-$CURSYNTH_BUNDLE}"
HOST="${2:-$CURSYNTH_HOST}"
MODULE=$(ls "$BUNDLE"/cursynth.* 2>/dev/null | grep -vE '\.ttl$|dSYM' | head -1)
if [ -z "$MODULE" ]; then
  echo "no plugin binary in $BUNDLE"
  exit 1
fi

DEFAULTS=$(mktemp)
awk '
  /lv2:index/   { idx=$2 }
  /lv2:default/ { d=$2 }
  /lv2:minimum/ { mn=$2 }
  /lv2:maximum/ { mx=$2; if (idx >= 3) print idx, d, mn, mx, "control" }
' "$BUNDLE/cursynth.ttl" | tr -d ';' > "$DEFAULTS"

n=$(wc -l < "$DEFAULTS" | tr -d ' ')
echo "  $n control ports read from the generated turtle"
if [ "$n" -lt 10 ]; then
  echo "  too few ports; the turtle file is not what it should be"
  rm -f "$DEFAULTS"
  exit 1
fi

VOL=$(awk '$0 ~ /volume/ { print $1 }' "$BUNDLE/cursynth.ttl" > /dev/null 2>&1; \
      grep -B3 '"volume"' "$BUNDLE/cursynth.ttl" | awk '/lv2:index/ { print $2 }' | tr -d ';' | head -1)
echo "  volume is port $VOL"

# Piping to sed would hand the pipeline sed's exit status and throw away the
# host's, so the output goes to a file and the status is looked at on its own.
OUT=$(mktemp)
run() {
  label="$1"
  shift
  echo "  $label"
  if "$HOST" "$@" > "$OUT" 2>&1; then
    sed 's/^/    /' "$OUT"
  else
    sed 's/^/    /' "$OUT"
    echo "  the host reported a failure"
    rm -f "$OUT" "$DEFAULTS"
    exit 1
  fi
}

run "at the default settings:"  "$MODULE" "$DEFAULTS"
run "with the volume down:"     "$MODULE" "$DEFAULTS" "$VOL" 0.0 0.0
run "with the volume up:"       "$MODULE" "$DEFAULTS" "$VOL" 1.0 0.0591

rm -f "$OUT" "$DEFAULTS"
