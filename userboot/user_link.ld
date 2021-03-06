OUTPUT_FORMAT("binary")    /* output flat binary */

SECTIONS
{
  /* This should be the same value as USER_START. */
  . = 1024M;
  __binary_start = .;

  .text ALIGN(4):
  {
    /* Ensure the user program entry comes first. */
    *(.user_program_entry)

    /* include all other .text sections */
    *(.text)
  }

  .data ALIGN(4):
  {
    *(.data)
  }

  .rodata ALIGN(4):
  {
    *(.rodata*)
  }

  /* Read-write data (uninitialized) and stack */
  .bss BLOCK(4K) : ALIGN(4K)
  {
    *(.bss)
  }

  __binary_end = .;
}
