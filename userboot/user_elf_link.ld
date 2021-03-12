ENTRY(_start)

SECTIONS {
  /* This should be the same value as USER_START. */
  . = 1024M;

  .text 1024M : {
    *(._start)
    *(.text)
  }

  .data ALIGN(4) : {
    *(.data)
  }

  .rodata ALIGN(4) : {
    *(.rodata)
    *(.rodata.*)
  }

  /* FIXME: These are here because the linker can insert these into the final
   * executable at the start, causing overlap with .text. Once we support
   * loading at an offset, these will not be needed.*/
  .interp : {
    *(.interp)
  }
  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  }
}
