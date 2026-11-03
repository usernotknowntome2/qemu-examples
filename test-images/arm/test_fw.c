/*
 * test_fw.c : bare-metal test firmware for the arm_board machine
 *
 * Proves, in order: reset via VTOR in flash -> stack in DTCM -> semihosting
 * output -> SysTick (core timer, system exception 15) -> external timer
 * at the timer 0 address delivering NVIC IRQ ARM_BOARD_TIMER0_IRQ.
 *
 * Build:
 *   arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -O1 -nostdlib \
 *       -ffreestanding -T test_fw.ld test_fw.c -o test_fw.elf
 */
#include <stdint.h>

/* Keep in sync with the board file (RM-VERIFY values) */
#define ARM_BOARD_NUM_IRQ     240
#define ARM_BOARD_TIMER0_BASE   0x400B0000u
#define ARM_BOARD_TIMER0_IRQ    96

#define REG32(a)         (*(volatile uint32_t *)(a))

/* Core registers (System Control Space) */
#define SYST_CSR         REG32(0xE000E010u)
#define SYST_RVR         REG32(0xE000E014u)
#define SYST_CVR         REG32(0xE000E018u)
#define NVIC_ISER(n)     REG32(0xE000E100u + 4u * (n))

/* Stand-in timer register layout (cmsdk-apb-timer), not the SoC's own timer */
#define TMR_CTRL         REG32(ARM_BOARD_TIMER0_BASE + 0x00u)
#define TMR_RELOAD       REG32(ARM_BOARD_TIMER0_BASE + 0x08u)
#define TMR_INTCLR       REG32(ARM_BOARD_TIMER0_BASE + 0x0Cu)

static volatile uint32_t systick_ticks;
static volatile uint32_t timer0_ticks;

/* ---- semihosting: zero-device console and clean exit ---- */
static inline int semihost(int op, const void *arg)
{
    register int r0 __asm__("r0") = op;
    register const void *r1 __asm__("r1") = arg;
    __asm__ volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}
static void puts_sh(const char *s) { semihost(0x04, s); }        /* SYS_WRITE0 */
static void exit_sh(void) { semihost(0x18, (void *)0x20026); }   /* SYS_EXIT */

static void put_u32(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    puts_sh(&b[i]);
}

/* ---- handlers ---- */
void SysTick_Handler(void) { systick_ticks++; }

void TIMER0_Handler(void)
{
    TMR_INTCLR = 1;         /* forget this and the IRQ never de-asserts */
    timer0_ticks++;
}

void Default_Handler(void)
{
    puts_sh("[arm_board] unexpected exception. FAIL\n");
    exit_sh();
    for (;;) { }
}

void Reset_Handler(void);
extern uint32_t _estack;

/* Vector table: 16 core entries + ARM_BOARD_NUM_IRQ external entries */
__attribute__((section(".isr_vector"), used))
void (*const vector_table[16 + ARM_BOARD_NUM_IRQ])(void) = {
    [0] = (void (*)(void))&_estack,     /* initial MSP */
    [1] = Reset_Handler,
    [2 ... 14] = Default_Handler,
    [15] = SysTick_Handler,
    [16 ... 16 + ARM_BOARD_NUM_IRQ - 1] = Default_Handler,
    [16 + ARM_BOARD_TIMER0_IRQ] = TIMER0_Handler,
};

int main(void)
{
    puts_sh("[arm_board] reset OK, VTOR at flash, stack in DTCM\n");

    /* SysTick: core clock source, interrupt enabled, 10000-cycle period */
    SYST_RVR = 10000 - 1;
    SYST_CVR = 0;
    SYST_CSR = (1u << 2) | (1u << 1) | (1u << 0);

    /* Stand-in timer 0: reload, enable IRQ + counter, then unmask in NVIC */
    TMR_RELOAD = 50000;
    TMR_CTRL = (1u << 3) | (1u << 0);
    NVIC_ISER(ARM_BOARD_TIMER0_IRQ / 32) = 1u << (ARM_BOARD_TIMER0_IRQ % 32);

    __asm__ volatile("cpsie i");

    while (systick_ticks < 50 || timer0_ticks < 10) {
        __asm__ volatile("wfi");
    }

    puts_sh("[arm_board] SysTick ticks: ");
    put_u32(systick_ticks);
    puts_sh(", TIMER0 (NVIC IRQ ");
    put_u32(ARM_BOARD_TIMER0_IRQ);
    puts_sh(") ticks: ");
    put_u32(timer0_ticks);
    puts_sh("\n[arm_board] PASS\n");
    exit_sh();
    return 0;
}

void Reset_Handler(void)
{
    extern uint32_t _sbss, _ebss;
    for (uint32_t *p = &_sbss; p < &_ebss; p++) {
        *p = 0;
    }
    main();
    for (;;) { }
}
