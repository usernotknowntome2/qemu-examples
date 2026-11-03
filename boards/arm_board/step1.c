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
    /* STEP 2: CPU */

    /* STEP 3: RAM and ROM */

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
