/*
 * arm_board: minimal ARM machine model (QEMU examples)
 *
 * CPU + on-chip memories (ITCM, flash, DTCM, SRAM) + NVIC + SysTick + timer
 *
 * Values tagged RM-VERIFY are starting values: check each one against the
 * memory map and interrupt map chapters of the SoC reference manual.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "hw/core/boards.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/sysbus.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/misc/unimp.h"
#include "system/address-spaces.h"
#include "target/arm/cpu-qom.h"

/* Memory map (Objective 1: memory map chapter of the reference manual) */
#define ARM_BOARD_ITCM_BASE     0x00000000
#define ARM_BOARD_ITCM_SIZE     (64 * KiB)        /* RM-VERIFY */
#define ARM_BOARD_FLASH_BASE    0x00400000
#define ARM_BOARD_FLASH_SIZE    (8 * MiB)         /* RM-VERIFY */
#define ARM_BOARD_DTCM_BASE     0x20000000
#define ARM_BOARD_DTCM_SIZE     (128 * KiB)       /* RM-VERIFY */
#define ARM_BOARD_SRAM_BASE     0x20400000
#define ARM_BOARD_SRAM_SIZE     (512 * KiB)       /* RM-VERIFY */
#define ARM_BOARD_PERIPH_BASE   0x40000000
#define ARM_BOARD_PERIPH_SIZE   0x00800000        /* RM-VERIFY */

/* Interrupts (Objective 1: interrupt map of the reference manual) */
#define ARM_BOARD_NUM_IRQ       240               /* RM-VERIFY */
#define ARM_BOARD_TIMER0_BASE   0x400B0000        /* RM-VERIFY */
#define ARM_BOARD_TIMER0_IRQ    96                /* RM-VERIFY */

/* Clock (clocking chapter; a functional model only needs a value) */
#define ARM_BOARD_CORE_HZ       160000000ULL

/* Board init: QEMU calls this once to build the machine */
static void arm_board_init(MachineState *ms)
{
    /* STEP 2: CPU. CPU + NVIC + SysTick come in one armv7m container */
    Clock *sysclk = clock_new(OBJECT(ms), "SYSCLK");
    clock_set_hz(sysclk, ARM_BOARD_CORE_HZ);

    DeviceState *armv7m = qdev_new(TYPE_ARMV7M);
    object_property_add_child(OBJECT(ms), "armv7m", OBJECT(armv7m));
    qdev_prop_set_string(armv7m, "cpu-type", ms->cpu_type);
    /* STEP 4: interrupt controller. NVIC is inside armv7m: set its size */
    qdev_prop_set_uint32(armv7m, "num-irq", ARM_BOARD_NUM_IRQ);
    /* Reset: CPU reads SP and PC from the vector table at this address */
    qdev_prop_set_uint32(armv7m, "init-nsvtor", ARM_BOARD_FLASH_BASE);
    qdev_connect_clock_in(armv7m, "cpuclk", sysclk);     /* mandatory */
    object_property_set_link(OBJECT(armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(armv7m), &error_fatal);

    /* Loads -kernel ELF and registers the CPU reset hook (always needed) */
    armv7m_load_kernel(ARMV7M(armv7m)->cpu, ms->kernel_filename,
                       ARM_BOARD_FLASH_BASE, ARM_BOARD_FLASH_SIZE);

    /* STEP 3: RAM and ROM. On-chip sizes are fixed by the silicon */
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *itcm = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *dtcm = g_new(MemoryRegion, 1);
    MemoryRegion *sram = g_new(MemoryRegion, 1);

    memory_region_init_ram(itcm, NULL, "arm_board.itcm", ARM_BOARD_ITCM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, ARM_BOARD_ITCM_BASE, itcm);

    memory_region_init_rom(flash, NULL, "arm_board.flash", ARM_BOARD_FLASH_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, ARM_BOARD_FLASH_BASE, flash);

    memory_region_init_ram(dtcm, NULL, "arm_board.dtcm", ARM_BOARD_DTCM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, ARM_BOARD_DTCM_BASE, dtcm);

    memory_region_init_ram(sram, NULL, "arm_board.sram", ARM_BOARD_SRAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(sysmem, ARM_BOARD_SRAM_BASE, sram);

    /* Peripherals not modelled yet: accesses are logged with -d unimp */
    create_unimplemented_device("arm_board.periph", ARM_BOARD_PERIPH_BASE,
                                ARM_BOARD_PERIPH_SIZE);

    /* STEP 5: timer and IRQ wiring */
}

static void arm_board_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m7"),
        NULL
    };

    mc->desc = "Minimal ARM example machine";
    mc->init = arm_board_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m7");
    mc->valid_cpu_types = valid_cpu_types;
    mc->max_cpus = 1;
}

DEFINE_MACHINE_ARM("arm_board", arm_board_machine_init)
