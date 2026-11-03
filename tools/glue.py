#!/usr/bin/env python3
"""
Register (or unregister) the two example boards in a QEMU source tree:
Kconfig entries + meson.build lines. Original files are backed up once as
<file>.qemu-examples.bak and restored by 'remove'.

Usage: glue.py add|remove <qemu-source-dir>
"""
import os
import shutil
import sys

action, qemu = sys.argv[1], sys.argv[2]
FILES = ['hw/i386/Kconfig', 'hw/i386/meson.build',
         'hw/arm/Kconfig', 'hw/arm/meson.build']
BOARDS = ['hw/i386/x86_board.c', 'hw/arm/arm_board.c']


def path(rel):
    return os.path.join(qemu, rel)


if action == 'remove':
    for rel in FILES:
        bak = path(rel) + '.qemu-examples.bak'
        if os.path.exists(bak):
            shutil.move(bak, path(rel))
            os.utime(path(rel))     # new timestamp, so QEMU's build notices
            print('restored  ' + rel)
    for rel in BOARDS:
        if os.path.exists(path(rel)):
            os.remove(path(rel))
            print('removed   ' + rel)
    sys.exit(0)

for rel in FILES:
    bak = path(rel) + '.qemu-examples.bak'
    if not os.path.exists(bak):
        shutil.copy2(path(rel), bak)

# ---- x86_board ----
kc = open(path('hw/i386/Kconfig')).read()
if 'config X86_BOARD' not in kc:
    kc = kc.rstrip('\n') + '''

config X86_BOARD
    bool
    default y
    depends on I386
    select ISA_BUS
    select APIC
    select I8259
    select I8254
    select SERIAL_ISA
'''
    open(path('hw/i386/Kconfig'), 'w').write(kc)
    print('added     hw/i386/Kconfig: config X86_BOARD')

mb = open(path('hw/i386/meson.build')).read()
if 'CONFIG_X86_BOARD' not in mb:
    files = "'x86_board.c'"
    if os.path.exists(path('hw/i386/x86-common.c')):
        files = "'x86-common.c', " + files
    line = "i386_ss.add(when: 'CONFIG_X86_BOARD', if_true: files(%s))\n" % files
    lines = mb.splitlines(keepends=True)
    # right after the first conditional line of the i386 source set
    idx = next(i for i, l in enumerate(lines)
               if l.startswith("i386_ss.add(when: 'CONFIG_"))
    lines.insert(idx, line)
    open(path('hw/i386/meson.build'), 'w').write(''.join(lines))
    print('added     hw/i386/meson.build: ' + line.strip())

# ---- arm_board ----
kc = open(path('hw/arm/Kconfig')).read()
if 'config ARM_BOARD' not in kc:
    kc = kc.rstrip('\n') + '''

config ARM_BOARD
    bool
    default y
    depends on TCG && ARM
    select ARM_V7M
    select CMSDK_APB_TIMER
    select UNIMP
'''
    open(path('hw/arm/Kconfig'), 'w').write(kc)
    print('added     hw/arm/Kconfig: config ARM_BOARD')

mb = open(path('hw/arm/meson.build')).read()
if 'CONFIG_ARM_BOARD' not in mb:
    lines = mb.splitlines(keepends=True)
    # same source set as QEMU's own M-profile container (armv7m.c)
    idx = next(i for i, l in enumerate(lines) if "files('armv7m.c')" in l)
    var = lines[idx].split('.add(')[0]
    line = "%s.add(when: 'CONFIG_ARM_BOARD', if_true: files('arm_board.c'))\n" % var
    lines.insert(idx + 1, line)
    open(path('hw/arm/meson.build'), 'w').write(''.join(lines))
    print('added     hw/arm/meson.build: ' + line.strip())
