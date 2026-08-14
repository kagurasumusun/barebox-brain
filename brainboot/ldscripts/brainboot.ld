/* SPDX-License-Identifier: GPL-2.0-or-later */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    DRAM : ORIGIN = 0x40200000, LENGTH = 768K
}

SECTIONS
{
    . = 0x40200000;

    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > DRAM

    .rodata : {
        *(.rodata*)
    } > DRAM

    .data : {
        *(.data*)
    } > DRAM

    _edata = .;

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > DRAM

    . = ALIGN(8);
    _stack_bottom = .;
    . = . + 0x10000;
    _stack_top = .;

    _end = .;
    ASSERT(_end <= 0x402C0000, "brainboot image exceeds 768 KiB at 0x40200000")
}
