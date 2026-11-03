#!/usr/bin/env python3
"""
Copy an example board file into a QEMU source tree, adjusting it to the
QEMU version found there (header locations and a few function signatures
changed between QEMU 9.x, 10.x and 11.x). The logic of the board does not
change, only the spelling.

Usage: adapt.py <board-step-file> <qemu-source-dir> <output-file>
"""
import os
import re
import sys

src, qemu, out = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(src).read()


def exists(rel):
    return (os.path.isfile(os.path.join(qemu, 'include', rel)) or
            os.path.isfile(os.path.join(qemu, rel)))


def find_header(name):
    """Return the text of a header wherever it lives in this tree, or ''."""
    for rel in (name, name.replace('hw/core/', 'hw/')):
        p = os.path.join(qemu, 'include', rel)
        if os.path.isfile(p):
            return open(p).read()
    return ''


# 1. Header locations
def fix_include(m):
    inc = m.group(1)
    if exists(inc):
        return m.group(0)
    candidates = []
    if inc.startswith('hw/core/'):
        candidates.append('hw/' + inc[len('hw/core/'):])
    if inc == 'system/address-spaces.h':
        candidates.append('exec/address-spaces.h')
    if inc.startswith('system/'):
        candidates.append('sysemu/' + inc[len('system/'):])
    for c in candidates:
        if exists(c):
            return '#include "%s"' % c
    if inc == 'hw/arm/machines-qom.h':
        return '@@DROP@@'               # older QEMU: not needed
    sys.stderr.write('WARNING: header %s not found in this QEMU tree\n' % inc)
    return m.group(0)


text = re.sub(r'#include "([^"]+)"', fix_include, text)
text = text.replace('@@DROP@@\n', '')

# 2. ARM machine macro (QEMU before 10.1 has only DEFINE_MACHINE)
if 'DEFINE_MACHINE_ARM' in text and not exists('hw/arm/machines-qom.h'):
    text = text.replace('DEFINE_MACHINE_ARM(', 'DEFINE_MACHINE(')

# 3. class_init signature (const void *data from QEMU 10.1)
obj = find_header('qom/object.h')
if obj and 'const void *data' not in obj:
    text = text.replace('const void *data)', 'void *data)')

# 3b. TypeInfo.interfaces became const in QEMU 10.1
if obj and 'const InterfaceInfo *interfaces' not in obj:
    text = text.replace('(const InterfaceInfo[])', '(InterfaceInfo[])')

# 4. load_image_targphys gained an Error ** argument
loader = find_header('hw/core/loader.h')
m = re.search(r'load_image_targphys\(([^;]*)\);', loader, re.S)
if m and 'Error' not in m.group(1):
    text = text.replace('X86_ROM_SIZE, NULL)', 'X86_ROM_SIZE)')

# 5. ISA IRQ registration was renamed in QEMU 8.1
isa = find_header('hw/isa/isa.h')
if isa and 'isa_bus_register_input_irqs' not in isa:
    text = text.replace('isa_bus_register_input_irqs(', 'isa_bus_irqs(')

open(out, 'w').write(text)
