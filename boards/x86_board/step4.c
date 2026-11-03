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
    /* STEP 2: CPU. Reset state: real mode, CS base 0xFFFF0000, IP 0xFFF0 */
    if (!tcg_enabled()) {
        error_report("x86_board: only TCG is supported (use -accel tcg)");
        exit(1);
    }
    x86_cpus_init(X86_MACHINE(ms), CPU_VERSION_LATEST);

    /* STEP 3: RAM and ROM */
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *bios = g_new(MemoryRegion, 1);
    MemoryRegion *bios_legacy = g_new(MemoryRegion, 1);

    /* RAM at 0x0, size comes from -m (default 16 MiB) */
    memory_region_add_subregion(sysmem, 0, ms->ram);

    /* 256 KiB BIOS flash at the top of 4 GiB: holds reset vector FFFFFFF0h */
    memory_region_init_rom(bios, NULL, "x86_board.bios", X86_BIOS_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, X86_BIOS_BASE, bios);

    /* Top 128 KiB of the flash again at 0xE0000-0xFFFFF (E and F segments).
     * Priority 1 beats the RAM underneath */
    memory_region_init_alias(bios_legacy, NULL, "x86_board.bios.legacy", bios,
                             X86_BIOS_SIZE - X86_LEGACY_SIZE, X86_LEGACY_SIZE);
    memory_region_add_subregion_overlap(sysmem, X86_LEGACY_BASE,
                                        bios_legacy, 1);

    /* Load the -bios file (a 256 KiB flash image) into the flash */
    if (!ms->firmware ||
        load_image_targphys(ms->firmware, X86_BIOS_BASE, X86_BIOS_SIZE, NULL) !=
        X86_BIOS_SIZE) {
        error_report("x86_board: pass -bios <256 KiB image>");
        exit(1);
    }

    /* STEP 4: interrupt controller */
    /* ISA bus: memory space + the separate x86 port I/O space */
    ISABus *isa_bus = isa_bus_new(NULL, sysmem, get_system_io(), &error_fatal);

    /* i8259 pair, master 20h-21h + slave A0h-A1h. Output -> CPU INTR pin */
    qemu_irq *pic = i8259_init(isa_bus, x86_allocate_cpu_irq());

    /* ISA IRQ n -> PIC input n */
    isa_bus_register_input_irqs(isa_bus, pic);

    /* COM1: 16550 UART at 3F8h on IRQ4, the console the test ROM prints to */
    ISADevice *com1 = isa_new("isa-serial");
    qdev_prop_set_uint32(DEVICE(com1), "iobase", X86_COM1_PORT);
    qdev_prop_set_uint32(DEVICE(com1), "irq", X86_COM1_IRQ);
    qdev_prop_set_chr(DEVICE(com1), "chardev", serial_hd(0));
    isa_realize_and_unref(com1, isa_bus, &error_fatal);

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
