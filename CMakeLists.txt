# This contains the build rules for making the kernel, and optionally the
# default userboot program and cdrom image.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel)
enable_language(CXX ASM)

set(KERNEL kernel)
set(IMAGE myos.iso)
set(INITRD initrd.vfs)
set(LINK_FLAGS
  -m elf_i386
  -T ${CMAKE_SOURCE_DIR}/src/link.ld)
#set(FONT font.o)

add_subdirectory(src)

# TODO: Move this to a user program.
## The object file provided by this will provided the symbols needed for fonts
## when using graphics:
## - _binary_font_psf_start
## - _binary_font_psf_end
## - _binary_font_psf_size
#add_custom_target(${FONT} ALL
#  # Copy the file into the current directory so objcopy doesn't include the
#  # path names in symbols in the final binary.
#  # TODO: Add an option to specify the psf that should be used.
#  COMMAND cp ${CMAKE_SOURCE_DIR}/font.psf ${CMAKE_CURRENT_BINARY_DIR}/font.psf
#  COMMAND ${CMAKE_OBJCOPY} -O elf32-i386 -B i386 -I binary font.psf ${FONT}
#  COMMENT "Create font object file"
#  VERBATIM)

add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_LINKER} ${LINK_FLAGS} -o ${KERNEL} --start-group $<TARGET_FILE:asm_objs> $<TARGET_FILE:cpp_objs> --end-group
  COMMAND grub-file --is-x86-multiboot ${KERNEL}
  COMMENT "Link ${KERNEL}"
  DEPENDS asm_objs cpp_objs
  VERBATIM)

# This adds the build rules for making initrd.vfs.
add_subdirectory(userboot)

# This takes the initrd.vfs sitting in the build directory and uses it. This
# initrd does not need to be from the userboot directory. It can be any initrd
# file that contains a file named "userboot" and has an entry point at USER_START.
add_custom_target(${IMAGE}
  COMMAND mkdir -p isodir/boot/grub
  COMMAND cp ${KERNEL} isodir/boot/${KERNEL}
  COMMAND cp ${CMAKE_SOURCE_DIR}/grub.cfg isodir/boot/grub/grub.cfg

  # Copy the ramdisk
  COMMAND cp ${INITRD} isodir/boot/${INITRD}

  COMMAND grub-mkrescue -o ${IMAGE} isodir
  DEPENDS grub.cfg ${INITRD} ${KERNEL}
  COMMENT "Create cdrom image"
  VERBATIM)
