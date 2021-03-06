# This contains the build rules for making the kernel, and optionally the
# default userboot program and cdrom image.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel)
enable_language(CXX ASM)

set(KERNEL kernel)
set(IMAGE myos.iso)

add_subdirectory(kernel)

# This adds the build rules for making the initial ramdisk.
set(INITRD initrd)
if(EXISTS ${CMAKE_SOURCE_DIR}/userboot)
  MESSAGE("Found userboot subdirectory. Using this to create initrd.")
  set(INITRD_DST_DIR ${CMAKE_CURRENT_BINARY_DIR})
  add_subdirectory(userboot)
endif()

# This takes the initrd sitting in the build directory and uses it. This
# initrd does not need to be from the userboot directory.
add_custom_target(${IMAGE}
  COMMAND mkdir -p isodir/boot/grub
  COMMAND cp kernel/${KERNEL} isodir/boot/${KERNEL}
  COMMAND cp ${CMAKE_SOURCE_DIR}/grub.cfg isodir/boot/grub/grub.cfg

  # Copy the ramdisk
  COMMAND cp ${INITRD} isodir/boot/${INITRD}

  COMMAND grub-mkrescue -o ${IMAGE} isodir
  DEPENDS grub.cfg ${INITRD} ${KERNEL}
  COMMENT "Create cdrom image"
  VERBATIM)
