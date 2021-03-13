/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
  /* Begin putting sections at 4 MiB */
  . = 4M;

  /* First put the multiboot header, as it is required to be put very early
     early in the image or the bootloader won't recognize the file format.
     Next we'll put the .text section. */
  .text : ALIGN(4K)
  {
    ASSERT(. == 4M, "Text does not start at 4MB");
    *(.multiboot)
    *(.text)
  }

  /* Read-only data. */
  .rodata : ALIGN(4K)
  {
    *(.rodata)
    *(.rodata.*)
  }

  /* Read-write data (initialized) */
  .data : ALIGN(4K)
  {
    *(.data)
  }

  /* Read-write data (uninitialized) and stack */
  .bss : ALIGN(4K)
  {
    *(COMMON)
    *(.bss)

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

  /* The compiler may produce other sections, by default it will put them in
     a segment with the same name. Simply add stuff here as needed. */
  end = .; _end = .;
}
