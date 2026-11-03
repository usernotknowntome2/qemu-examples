; test_rom.asm : 64 KiB test ROM for the x86_board machine
;
; Mapped by the board at 0xFFFF0000 (top of 4 GiB) and aliased at 0xF0000.
; Proves, in order: reset vector -> ROM alias -> RAM -> PIC -> PIT -> IRQ0.
;
; Build: nasm -f bin test_rom.asm -o test_rom.bin   (must be exactly 65536 bytes)
; Run:   qemu-system-x86_64 -M x86_board -accel tcg -bios test_rom.bin -nographic

bits 16
org 0x0000                  ; offsets inside segment F000

DEBUGCON    equ 0xE9        ; isa-debugcon port
TICKS       equ 0x0500      ; tick counter in RAM (segment 0)
NTICKS      equ 20          ; ticks to wait before declaring success
PIT_HZ      equ 1193182
PIT_DIV     equ PIT_HZ / 100    ; ~100 Hz

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

    ; --- i8254 channel 0: lobyte/hibyte, mode 2 (rate generator) ---
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
    out  DEBUGCON, al
    mov  al, 0x20           ; non-specific EOI, or we get exactly one tick
    out  0x20, al
    pop  ds
    pop  ax
    iret

; puts: print zero-terminated string at CS:SI to the debug port
puts:
    push ax
.next:
    mov  al, [cs:si]
    inc  si
    test al, al
    jz   .out
    out  DEBUGCON, al
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
    jmp  0xF000:start       ; far jump: CS base becomes 0xF0000 (low alias)
    times 0x10000 - ($ - $$) db 0xFF
