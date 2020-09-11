OUTPUT_FORMAT("binary")    /* output flat binary */

SECTIONS
{
  /* This should be the same value as USER_START. */
  . = 1024M;

  .text ALIGN(4):
  {
    start_user_program.o(.text)     /* include the .text section of start.o */
    *(.text)           /* include all other .text sections */
  }

  .data ALIGN(4):
  {
    *(.data)
  }

  .rodata ALIGN(4):
  {
    *(.rodata*)
  }
}
