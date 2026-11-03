; test_rom.asm : 256 KiB BIOS flash image for the x86_board machine
;
; The board maps it at 0xFFFC0000-0xFFFFFFFF (top of 4 GiB) and maps its
; top 128 KiB again at 0xE0000-0xFFFFF. The code lives in the top 64 KiB,
; which is segment F000 below 1 MiB.
; Proves, in order: reset vector -> legacy alias -> RAM -> COM1 -> PIC -> PIT -> IRQ0.
;
; Build: nasm -f bin test_rom.asm -o test_rom.bin   (must be exactly 262144 bytes)
; Run:   qemu-system-x86_64 -M x86_board -accel tcg -bios test_rom.bin -nographic

bits 16

COM1        equ 0x3F8       ; serial port 1: data register (THR)
COM1_LSR    equ COM1 + 5    ; line status register, bit 5 = THR empty
TICKS       equ 0x0500      ; tick counter in RAM (segment 0)
NTICKS      equ 20          ; ticks to wait before declaring success
PIT_HZ      equ 1193182
PIT_DIV     equ PIT_HZ / 100    ; ~100 Hz

; ---- bottom 192 KiB of the flash: unused (erased flash reads 0xFF) ----
section unused start=0
    times 0x30000 db 0xFF

; ---- top 64 KiB: segment F000 (0xF0000 below 1 MiB, 0xFFFF0000 below 4 GiB) ----
section code start=0x30000 vstart=0

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  ss, ax
    mov  sp, 0x7000         ; stack in RAM
    mov  word [TICKS], 0

    mov  si, msg_boot
    call puts

    ; The local APIC comes out of reset enabled, with LVT LINT0 MASKED.
    ; A real BIOS then sets LINT0 to ExtINT ("virtual wire mode").
    ; We take the simpler legacy route: disable the LAPIC (IA32_APIC_BASE
    ; bit 11), so the PIC INTR line drives the CPU directly.
    ; Remove this block and IRQ0 sits pending in the PIC forever.
    mov  ecx, 0x1B          ; IA32_APIC_BASE MSR
    rdmsr
    and  eax, ~(1 << 11)    ; clear the global enable bit
    wrmsr

    ; IVT entry for vector 0x08 (IRQ0 after we program the PIC base to 0x08)
    mov  word [0x08*4],   timer_isr
    mov  word [0x08*4+2], 0xF000

    ; --- i8259 master: ICW1..ICW4, then OCW1 mask ---
    mov  al, 0x11           ; ICW1: edge, cascade, ICW4 needed
    out  0x20, al
    mov  al, 0x08           ; ICW2: vector base 0x08
    out  0x21, al
    mov  al, 0x04           ; ICW3: slave on IR2
    out  0x21, al
    mov  al, 0x01           ; ICW4: 8086 mode
    out  0x21, al
    mov  al, 0xFE           ; OCW1: unmask IR0 only
    out  0x21, al

    ; --- i8254 counter 0: lobyte/hibyte, mode 2 (rate generator) ---
    mov  al, 0x34
    out  0x43, al
    mov  ax, PIT_DIV
    out  0x40, al
    mov  al, ah
    out  0x40, al

    mov  si, msg_wait
    call puts
    sti

.idle:
    hlt                     ; sleep until the next interrupt
    cmp  word [TICKS], NTICKS
    jb   .idle

    cli
    mov  si, msg_done
    call puts
.stop:
    hlt
    jmp  .stop

; IRQ0 handler: count, print '.', EOI
timer_isr:
    push ax
    push ds
    xor  ax, ax
    mov  ds, ax
    inc  word [TICKS]
    mov  al, '.'
    call putc
    mov  al, 0x20           ; non-specific EOI, or we get exactly one tick
    out  0x20, al
    pop  ds
    pop  ax
    iret

; putc: send AL to COM1 (wait until the transmit register is empty)
putc:
    push dx
    push ax
    mov  dx, COM1_LSR
.wait:
    in   al, dx
    test al, 0x20
    jz   .wait
    pop  ax
    mov  dx, COM1
    out  dx, al
    pop  dx
    ret

; puts: print zero-terminated string at CS:SI to COM1
puts:
    push ax
.next:
    mov  al, [cs:si]
    inc  si
    test al, al
    jz   .out
    call putc
    jmp  .next
.out:
    pop  ax
    ret

msg_boot db "[x86_board] reset vector OK, running from F000 alias", 13, 10, 0
msg_wait db "[x86_board] PIC+PIT programmed, waiting for IRQ0 ", 0
msg_done db 13, 10, "[x86_board] 20 timer interrupts received. PASS", 13, 10, 0

; ---- reset vector: CPU starts at 0xFFFFFFF0 (CS base 0xFFFF0000, IP FFF0) ----
    times 0xFFF0 - ($ - $$) db 0xFF
reset:
    jmp  0xF000:start       ; far jump: CS base becomes 0xF0000 (legacy alias)
    times 0x10000 - ($ - $$) db 0xFF
