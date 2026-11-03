#!/bin/bash
# Setup. Run ONCE, before first use.
#   ./setup.sh <qemu-source-folder> [qemu-build-folder]
# Example:
#   ./setup.sh ~/qemu            (build folder defaults to ~/qemu/build)
set -e
KIT="$(cd "$(dirname "$0")" && pwd)"

if [ -z "$1" ]; then
    echo "Usage: ./setup.sh <qemu-source-folder> [qemu-build-folder]"
    exit 1
fi
Q="$(cd "$1" && pwd)"
B="$(cd "${2:-$Q/build}" && pwd)"

if [ ! -f "$Q/VERSION" ] || [ ! -d "$Q/hw/i386" ]; then
    echo "ERROR: $Q does not look like a QEMU source folder."
    exit 1
fi
if [ ! -f "$B/build.ninja" ]; then
    echo "ERROR: $B is not a configured QEMU build folder."
    echo "       Pass the build folder as the second argument."
    exit 1
fi
command -v python3 >/dev/null || { echo "ERROR: python3 not found."; exit 1; }

# Which QEMU binaries did this person build?
TARGETS=""
[ -x "$B/qemu-system-x86_64" ] && TARGETS="$TARGETS qemu-system-x86_64"
if [ -x "$B/qemu-system-arm" ]; then
    TARGETS="$TARGETS qemu-system-arm"
elif [ -x "$B/qemu-system-aarch64" ]; then
    TARGETS="$TARGETS qemu-system-aarch64"
fi
if [ -z "$TARGETS" ]; then
    echo "ERROR: no qemu-system-x86_64, qemu-system-arm or qemu-system-aarch64"
    echo "       found in $B. Build QEMU first."
    exit 1
fi
case "$TARGETS" in *arm*|*aarch64*)
    if ! grep -q "init-nsvtor" "$Q/hw/arm/armv7m.c"; then
        echo "WARNING: this QEMU is too old for the ARM track (needs QEMU 8.1 or newer)."
        TARGETS="$(echo $TARGETS | tr ' ' '\n' | grep -v -e arm -e aarch64 | tr '\n' ' ')"
    fi;;
esac

echo "QEMU source : $Q  (version $(cat "$Q/VERSION"))"
echo "QEMU build  : $B"
echo "Will build  :$TARGETS"
printf 'Q="%s"\nB="%s"\nTARGETS="%s"\n' "$Q" "$B" "$TARGETS" > "$KIT/.qemu-paths"

python3 "$KIT/tools/glue.py" add "$Q"
echo
echo "First build after setup re-runs QEMU's configuration: a few minutes."
exec "$KIT/step.sh" 1
