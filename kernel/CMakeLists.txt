add_library(asm_objs STATIC boot.s gdt.s interrupt.s task.s)
target_compile_options(asm_objs
  PRIVATE -target i686)

add_library(cpp_objs STATIC
  kernel.cpp DescriptorTables.cpp kstring.cpp isr.cpp Timer.cpp
  paging.cpp panic.cpp assert.cpp tests.cpp kmalloc.cpp task.cpp
  syscall.cpp keyboard.cpp ktype.cpp kstdlib.cpp builtins.cpp kctype.cpp
  serial.cpp cxa_pure_virtual.cpp new.cpp vfs.cpp stacktrace.cpp print.cpp)
target_include_directories(cpp_objs PRIVATE include/)
target_compile_options(cpp_objs
  PRIVATE -target i386
  PRIVATE -ffreestanding
  PRIVATE -Wall
  PRIVATE -Wextra
  PRIVATE -Werror
  PRIVATE -Wconversion
  PRIVATE -Wno-sign-compare
  PRIVATE -Wno-sign-conversion
  PRIVATE -fno-exceptions
  PRIVATE -fno-rtti
  PRIVATE -ftrivial-auto-var-init=pattern
  PRIVATE -std=c++17
  PRIVATE -UNDEBUG  # We keep all assert()s

  # This is needed for stack tracing.
  PRIVATE -fno-omit-frame-pointer

  PRIVATE -nostdlib)

# TODO: Move this to a user program.
## The object file provided by this will provided the symbols needed for fonts
## when using graphics:
## - _binary_font_psf_start
## - _binary_font_psf_end
## - _binary_font_psf_size
#set(FONT font.o)
#add_custom_target(${FONT} ALL
#  # Copy the file into the current directory so objcopy doesn't include the
#  # path names in symbols in the final binary.
#  # TODO: Add an option to specify the psf that should be used.
#  COMMAND cp ${CMAKE_SOURCE_DIR}/font.psf ${CMAKE_CURRENT_BINARY_DIR}/font.psf
#  COMMAND ${CMAKE_OBJCOPY} -O elf32-i386 -B i386 -I binary font.psf ${FONT}
#  COMMENT "Create font object file"
#  VERBATIM)

set(LINK_FLAGS
  -m elf_i386
  -T ${CMAKE_SOURCE_DIR}/kernel/link.ld)

add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_LINKER} ${LINK_FLAGS} -o ${KERNEL} --start-group $<TARGET_FILE:asm_objs> $<TARGET_FILE:cpp_objs> --end-group
  COMMAND grub-file --is-x86-multiboot ${KERNEL}
  COMMENT "Link ${KERNEL}"
  DEPENDS asm_objs cpp_objs
  VERBATIM)
