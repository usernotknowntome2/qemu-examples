#!/bin/bash
# Install the board files for one step and rebuild QEMU.
#   ./step.sh 1     (steps 1 to 5)
KIT="$(cd "$(dirname "$0")" && pwd)"
if [ ! -f "$KIT/.qemu-paths" ]; then
    echo "Run ./setup.sh first."
    exit 1
fi
. "$KIT/.qemu-paths"

N="$1"
case "$N" in
    1|2|3|4|5) ;;
    *) echo "Usage: ./step.sh <1-5>"; exit 1;;
esac

python3 "$KIT/tools/adapt.py" "$KIT/boards/x86_board/step$N.c" "$Q" \
        "$Q/hw/i386/x86_board.c" || exit 1
python3 "$KIT/tools/adapt.py" "$KIT/boards/arm_board/step$N.c" "$Q" \
        "$Q/hw/arm/arm_board.c" || exit 1
echo "Step $N files installed:"
echo "   $Q/hw/i386/x86_board.c"
echo "   $Q/hw/arm/arm_board.c"

echo "Building:$TARGETS ..."
if ! make -C "$B" $TARGETS > "$KIT/build.log" 2>&1; then
    echo
    echo "BUILD FAILED. Last lines of $KIT/build.log:"
    tail -n 25 "$KIT/build.log"
    exit 1
fi
echo "Step $N built OK. Machines available:"
for t in $TARGETS; do
    "$B/$t" -M help | grep -E "^(x86_board|arm_board) "
done
