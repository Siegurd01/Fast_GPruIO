/* Minimal AM335x PRU linker command file for remoteproc. */

MEMORY
{
    PAGE 0:
        PRU_IMEM    : org = 0x00000000 len = 0x00002000

    PAGE 1:
        PRU_DMEM    : org = 0x00000000 len = 0x00002000
}

SECTIONS
{
    .text           >  PRU_IMEM, PAGE 0
    .stack          >  PRU_DMEM, PAGE 1
    .bss            >  PRU_DMEM, PAGE 1
    .data           >  PRU_DMEM, PAGE 1
    .rodata         >  PRU_DMEM, PAGE 1
    .cinit          >  PRU_DMEM, PAGE 1
    .const          >  PRU_DMEM, PAGE 1
    .resource_table >  PRU_DMEM, PAGE 1
}
