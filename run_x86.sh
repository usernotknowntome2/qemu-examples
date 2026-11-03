#!/bin/bash
# Boot the x86 test ROM on the x86_board machine.
# Extra QEMU options can be added, for example:  ./run_x86.sh -S
# Quit QEMU: press Ctrl-a, then x.   Monitor: Ctrl-a, then c.
KIT="$(cd "$(dirname "$0")" && pwd)"
. "$KIT/.qemu-paths" || exit 1
exec "$B/qemu-system-x86_64" -M x86_board -accel tcg -nographic \
     -bios "$KIT/test-images/x86/test_rom.bin" "$@"
