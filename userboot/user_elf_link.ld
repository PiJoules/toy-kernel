ENTRY(_start)

SECTIONS {
  /* This should be the same value as USER_START. */
  . = 1024M;

  .text ALIGN(4) : {
    *(._start)
    *(.text)
  }

  .data ALIGN(4) : {
    *(.data)
  }

  .rodata ALIGN(4) : {
    *(.rodata)
  }
}
