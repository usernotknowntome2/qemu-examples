#!/bin/bash
# Boot the ARM test firmware on the arm_board machine.
# Extra QEMU options can be added, for example:  ./run_arm.sh -S
# Quit QEMU: press Ctrl-a, then x.   Monitor: Ctrl-a, then c.
KIT="$(cd "$(dirname "$0")" && pwd)"
. "$KIT/.qemu-paths" || exit 1
BIN="$B/qemu-system-arm"
[ -x "$BIN" ] || BIN="$B/qemu-system-aarch64"
exec "$BIN" -M arm_board -nographic \
     -kernel "$KIT/test-images/arm/test_fw.elf" \
     -semihosting-config enable=on,target=native "$@"
