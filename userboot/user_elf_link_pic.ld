ENTRY(_start)

SECTIONS {
  .data ALIGN(4) : {
    *(.data)
  }

  .rodata ALIGN(4) : {
    *(.rodata)
    *(.rodata.*)
  }

  .bss ALIGN(4) : {
    *(.bss)
    *(.bss.*)
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

  .text : {
    *(._start)
    *(.text)
    *(.text.*)
  }
}
