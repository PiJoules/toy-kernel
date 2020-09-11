cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel)
enable_language(CXX ASM)

set(KERNEL kernel)
set(IMAGE myos.iso)
set(LINK_FLAGS
  -m elf_i386
  -T ${CMAKE_SOURCE_DIR}/src/link.ld)
set(FONT font.o)
set(USER_PROGRAM user_program.bin)

add_subdirectory(src)

# The object file provided by this will provided the symbols needed for fonts
# when using graphics:
# - _binary_font_psf_start
# - _binary_font_psf_end
# - _binary_font_psf_size
add_custom_target(${FONT} ALL
  # Copy the file into the current directory so objcopy doesn't include the
  # path names in symbols in the final binary.
  # TODO: Add an option to specify the psf that should be used.
  COMMAND cp ${CMAKE_SOURCE_DIR}/font.psf ${CMAKE_CURRENT_BINARY_DIR}/font.psf
  COMMAND ${CMAKE_OBJCOPY} -O elf32-i386 -B i386 -I binary font.psf ${FONT}
  COMMENT Create font object file
  VERBATIM)

add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_LINKER} ${LINK_FLAGS} -o ${KERNEL} --start-group $<TARGET_FILE:asm_objs> $<TARGET_FILE:cpp_objs> ${FONT} --end-group
  COMMAND grub-file --is-x86-multiboot ${KERNEL}
  COMMENT Link ${KERNEL}
  DEPENDS asm_objs cpp_objs ${FONT}
  VERBATIM)

add_subdirectory(usersdk)

add_custom_target(${IMAGE}
  COMMAND mkdir -p isodir/boot/grub
  COMMAND cp ${KERNEL} isodir/boot/${KERNEL}
  COMMAND cp ${CMAKE_SOURCE_DIR}/grub.cfg isodir/boot/grub/grub.cfg
  COMMAND cp ${CMAKE_SOURCE_DIR}/test.data isodir/boot/test.data
  COMMAND cp usersdk/${USER_PROGRAM} isodir/boot/${USER_PROGRAM}
  COMMAND grub-mkrescue -o ${IMAGE} isodir
  DEPENDS grub.cfg ${KERNEL} ${USER_PROGRAM}
  COMMENT Create cdrom image
  VERBATIM)
