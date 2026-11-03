#!/bin/bash
# Undo everything setup.sh did to the QEMU source folder, then rebuild.
KIT="$(cd "$(dirname "$0")" && pwd)"
. "$KIT/.qemu-paths" || exit 1
python3 "$KIT/tools/glue.py" remove "$Q"
echo "Rebuilding:$TARGETS (this can take as long as setup did) ..."
if make -C "$B" $TARGETS > "$KIT/build.log" 2>&1; then
    echo "Done. QEMU is back to how it was."
else
    echo "Rebuild failed, see $KIT/build.log"
    exit 1
fi
