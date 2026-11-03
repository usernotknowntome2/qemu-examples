/*
 * x86_board: minimal x86 machine model (QEMU examples)
 *
 * CPU + RAM + BIOS flash + interrupt controller (i8259 PIC)
 * + timer (i8254 PIT) + serial port (16550 UART)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/core/boards.h"
#include "hw/core/loader.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/core/hotplug.h"
#include "hw/i386/x86.h"
#include "hw/isa/isa.h"
#include "hw/intc/i8259.h"
#include "hw/timer/i8254.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "system/tcg.h"
#include "target/i386/cpu.h"

/*
 * Memory map (chipset datasheet, memory decode ranges table)
 *   FFFC0000h-FFFFFFFFh  BIOS range, always decoded: holds the reset vector
 *   000E0000h-000FFFFFh  legacy BIOS ranges: the same flash below 1 MiB
 */
#define X86_BIOS_SIZE       (256 * KiB)
#define X86_BIOS_BASE       (4 * GiB - X86_BIOS_SIZE)     /* 0xFFFC0000 */
#define X86_LEGACY_SIZE     (128 * KiB)
#define X86_LEGACY_BASE     (1 * MiB - X86_LEGACY_SIZE)   /* 0x000E0000 */

/* I/O ports (chipset datasheet, fixed and variable I/O ranges tables) */
#define X86_PIT_PORT        0x40    /* 8254 timer, 40h-43h */
#define X86_PIT_IRQ         0       /* 8254 counter 0 -> IRQ0 */
#define X86_COM1_PORT       0x3F8   /* serial port 1 (COM1), 3F8h-3FFh */
#define X86_COM1_IRQ        4       /* COM1 interrupt line (PC convention) */

#define TYPE_X86_BOARD_MACHINE MACHINE_TYPE_NAME("x86_board")

/* The x86 machine base class owns CPU topology and APIC IDs via these hooks */
static void x86_board_pre_plug(HotplugHandler *hd, DeviceState *dev,
                                    Error **errp)
{
    x86_cpu_pre_plug(hd, dev, errp);
}

static void x86_board_plug(HotplugHandler *hd, DeviceState *dev,
                                Error **errp)
{
    x86_cpu_plug(hd, dev, errp);
}

static HotplugHandler *x86_board_get_hotplug_handler(MachineState *ms,
                                                          DeviceState *dev)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_CPU)) {
        return HOTPLUG_HANDLER(ms);
    }
    return NULL;
}

/* Board init: QEMU calls this once to build the machine */
static void x86_board_init(MachineState *ms)
{
    /* STEP 2: CPU */

    /* STEP 3: RAM and ROM */

    /* STEP 4: interrupt controller */

    /* STEP 5: timer and IRQ wiring */
}

static void x86_board_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(oc);

    mc->desc = "Minimal x86 example machine";
    mc->init = x86_board_init;
    mc->max_cpus = 1;
    mc->default_ram_id = "x86_board.ram";
    mc->default_ram_size = 16 * MiB;
    mc->default_cpu_type = TARGET_DEFAULT_CPU_TYPE;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
    mc->get_hotplug_handler = x86_board_get_hotplug_handler;
    hc->pre_plug = x86_board_pre_plug;
    hc->plug = x86_board_plug;
}

static const TypeInfo x86_board_machine_info = {
    .name       = TYPE_X86_BOARD_MACHINE,
    .parent     = TYPE_X86_MACHINE,
    .class_init = x86_board_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { TYPE_HOTPLUG_HANDLER },
        { }
    },
};

static void x86_board_register_types(void)
{
    type_register_static(&x86_board_machine_info);
}

type_init(x86_board_register_types)
