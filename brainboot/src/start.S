/* SPDX-License-Identifier: GPL-2.0-or-later */
    .syntax unified
    .arch armv5te
    .cpu arm926ej-s
    .arm

    .section .text.start, "ax", %progbits
    .global _start
    .type _start, %function
_start:
    /* Match the stock CE image: first word is a branch over the header. */
    b       real_start

    .align  2
    .space  0x3c - (. - _start)

    /* 0x40: ECEC placeholder; tools/mkimage.py patches this if requested. */
    .global ecec_header
ecec_header:
    .word   0
    .word   0
    .word   0
    .word   0

    .align  5
    .global real_start
real_start:
    mrs     r0, cpsr
    bic     r0, r0, #0x1f
    orr     r0, r0, #0xd3          /* SVC, IRQ/FIQ disabled */
    msr     cpsr_c, r0

    /* Clean+invalidate D-cache by set/way until empty. */
1:  mrc     p15, 0, r15, c7, c14, 3
    bne     1b
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0  /* invalidate I-cache */
    mcr     p15, 0, r0, c7, c10, 4 /* drain write buffer */
    mcr     p15, 0, r0, c8, c7, 0  /* invalidate TLBs */

    /* Disable MMU, D-cache; keep I-cache off until we decide. */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x0005        /* C, M */
    bic     r0, r0, #0x1000        /* I */
    bic     r0, r0, #0x0002        /* A */
    mcr     p15, 0, r0, c1, c0, 0

    ldr     sp, =_stack_top
    bic     sp, sp, #7

    /* Relocate if we were loaded somewhere other than link address.
     * r4 = runtime _start, r5 = linked _start. */
    adr     r4, _start
    ldr     r5, =_start
    cmp     r4, r5
    beq     3f
    ldr     r6, =_edata
    sub     r6, r6, r5
    mov     r0, r5
    mov     r1, r4
2:  ldr     r2, [r1], #4
    str     r2, [r0], #4
    subs    r6, r6, #4
    bgt     2b
    ldr     r0, =3f
    bx      r0

3:
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
4:  cmp     r0, r1
    bge     5f
    str     r2, [r0], #4
    b       4b
5:
    ldr     r0, =main
    mov     lr, pc
    bx      r0

    .global hang_forever
hang_forever:
    b       hang_forever

    /* Jump to an OS image. r0/r1/r2 already set by caller. r3 = entry. */
    .global jump_os
    .type jump_os, %function
jump_os:
    mov     r4, r3
    /* Disable caches and MMU again, drain. */
    mrc     p15, 0, r5, c1, c0, 0
    bic     r5, r5, #0x0005
    bic     r5, r5, #0x1000
    mcr     p15, 0, r5, c1, c0, 0
    mov     r5, #0
    mcr     p15, 0, r5, c7, c5, 0
    mcr     p15, 0, r5, c7, c10, 4
    mcr     p15, 0, r5, c8, c7, 0
    bx      r4

    /*
     * copy_and_jump(src, dst, size, entry)
     * Copies the trampoline to 0x47F00000 then runs it so we can
     * overwrite our own image at dst (typically 0x40200000).
     */
    .global copy_and_jump
    .type copy_and_jump, %function
copy_and_jump:
    adr     r4, tramp_begin
    adr     r5, tramp_end
    ldr     r6, =0x47F00000
    mov     r7, r6
1:  ldr     r8, [r4], #4
    str     r8, [r6], #4
    cmp     r4, r5
    blo     1b
    bx      r7

tramp_begin:
    /* r0=src r1=dst r2=size r3=entry  — running from 0x47F00000 */
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
2:  ldr     r7, [r4], #4
    str     r7, [r5], #4
    subs    r6, r6, #4
    bgt     2b
    mrc     p15, 0, r7, c1, c0, 0
    bic     r7, r7, #0x0005
    bic     r7, r7, #0x1000
    mcr     p15, 0, r7, c1, c0, 0
    mov     r7, #0
    mcr     p15, 0, r7, c7, c5, 0
    mcr     p15, 0, r7, c7, c10, 4
    mov     r0, #0
    mov     r1, #0
    mov     r2, #0
    bx      r3
tramp_end:
    .word   0

    .size _start, . - _start
