/*
 * x86_board: minimal x86 machine model (QEMU examples)
 *
 * CPU + RAM + ROM + interrupt controller (i8259 PIC) + timer (i8254 PIT)
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

/* Memory map (Objective 1: x86 reset vector + PC platform layout) */
#define X86_ROM_SIZE        (64 * KiB)
#define X86_ROM_BASE        (4 * GiB - X86_ROM_SIZE)   /* 0xFFFF0000 */
#define X86_ROM_LOW_BASE    (1 * MiB - X86_ROM_SIZE)   /* 0x000F0000 */
#define X86_DEBUGCON_PORT   0xE9

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
    /* STEP 2: CPU. Reset state: real mode, CS base 0xFFFF0000, IP 0xFFF0 */
    if (!tcg_enabled()) {
        error_report("x86_board: only TCG is supported (use -accel tcg)");
        exit(1);
    }
    x86_cpus_init(X86_MACHINE(ms), CPU_VERSION_LATEST);

    /* STEP 3: RAM and ROM */
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *rom = g_new(MemoryRegion, 1);
    MemoryRegion *rom_low = g_new(MemoryRegion, 1);

    /* RAM at 0x0, size comes from -m (default 16 MiB) */
    memory_region_add_subregion(sysmem, 0, ms->ram);

    /* 64 KiB ROM at the top of 4 GiB: holds the reset vector 0xFFFFFFF0 */
    memory_region_init_rom(rom, NULL, "x86_board.rom", X86_ROM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, X86_ROM_BASE, rom);

    /* Same ROM again at 0xF0000 (real mode F000:xxxx). Priority 1 beats RAM */
    memory_region_init_alias(rom_low, NULL, "x86_board.rom.low", rom, 0,
                             X86_ROM_SIZE);
    memory_region_add_subregion_overlap(sysmem, X86_ROM_LOW_BASE, rom_low, 1);

    /* Load the -bios file into the ROM */
    if (!ms->firmware ||
        load_image_targphys(ms->firmware, X86_ROM_BASE, X86_ROM_SIZE, NULL) !=
        X86_ROM_SIZE) {
        error_report("x86_board: pass -bios <64 KiB image>");
        exit(1);
    }

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
