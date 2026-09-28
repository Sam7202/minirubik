/* link.ld - flat image for Ripes.
 *
 * Ripes' "bin" input is a raw binary: it is loaded at address 0 and execution
 * starts at 0, with no ELF headers read and no entry point honoured. So
 * everything is laid out contiguously from 0 with _start first (.text.init),
 * and the image is produced with objcopy -O binary. .bss and the stack sit
 * past the end of the image; Ripes' memory is sparse, so they need no bytes
 * in the file - _start zeroes .bss itself.
 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x00000000;

    .text : {
        *(.text.init)       /* _start must be the first thing in the image */
        *(.text .text.*)
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(8);
    __global_pointer$ = . + 0x800;

    .data : ALIGN(4) {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    . = ALIGN(4);
    __bss_start = .;
    .bss : {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(4);
    __bss_end = .;

    . = ALIGN(16);
    . = . + 16K;            /* stack, grows down from __stack_top */
    __stack_top = .;
}
