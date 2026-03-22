/*
 * openfpgaOS Application Linker Script
 *
 * Supports optional BRAM hot-path: mark functions with OF_FASTTEXT
 * to place them in on-chip BRAM (~51KB, zero wait state).
 * Apps that don't use OF_FASTTEXT are unaffected — BRAM sections are empty.
 *
 * The OS ELF loader detects BRAM-targeted PT_LOAD segments and copies
 * them to BRAM at load time.
 */

ENTRY(_start)

MEMORY {
    APP_BRAM (rwx) : ORIGIN = 0x00002000, LENGTH = 0xCC00
    SDRAM    (rwx) : ORIGIN = 0x10400000, LENGTH = 48M
}

SECTIONS {
    /* === SDRAM: main application code === */
    . = 0x10400000;

    .text : {
        *(.text.start)
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
    } > SDRAM

    /* === BRAM: hot code (VMA in BRAM, stored in ELF after SDRAM .text) === */
    .app_fasttext : {
        . = ALIGN(4);
        __app_fasttext_start = .;
        *(.app_fasttext*)
        *(.app_fastrodata*)
        . = ALIGN(4);
        __app_fasttext_end = .;
    } > APP_BRAM AT> SDRAM

    .app_fastdata : {
        . = ALIGN(4);
        __app_fastdata_start = .;
        *(.app_fastdata*)
        . = ALIGN(4);
        __app_fastdata_end = .;
    } > APP_BRAM AT> SDRAM

    /* === SDRAM: data sections === */
    .data : {
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
    } > SDRAM

    .dynamic : {
        *(.dynamic)
    } > SDRAM

    .rela.dyn : {
        *(.rela*)
    } > SDRAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SDRAM
}
