# This contains the build rules for making the kernel.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-kernel)
enable_language(CXX ASM)

add_library(asm_objs STATIC boot.s gdt.s interrupt.s task.s)
target_compile_options(asm_objs
  PRIVATE -target i386)

add_library(cpp_objs STATIC
  kernel.cpp DescriptorTables.cpp
  isr.cpp Timer.cpp
  paging.cpp panic.cpp kassert.cpp tests.cpp kmalloc.cpp task.cpp
  syscall.cpp keyboard.cpp
  kstdlib.cpp
  builtins.cpp
  serial.cpp cxa_pure_virtual.cpp new.cpp vfs.cpp stacktrace.cpp print.cpp)
target_include_directories(cpp_objs PRIVATE include/)
set(KERNEL_COMPILE_FLAGS
  -target i386
  -ffreestanding
  -Wall
  -Wextra
  -Werror
  -Wconversion
  -Wno-sign-compare
  -Wno-sign-conversion
  -fno-exceptions
  -fno-rtti
  -ftrivial-auto-var-init=pattern
  -std=c++17
  -UNDEBUG  # We keep all assert()s

  # This is needed for stack tracing.
  -fno-omit-frame-pointer

  -nostdlib)

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)
set(BUILD_KERNEL_LIBC 1)
add_subdirectory(${LIBC_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kernel_libc_build)

target_compile_options(cpp_objs PRIVATE ${KERNEL_COMPILE_FLAGS})
target_include_directories(cpp_objs PRIVATE ${LIBC_PROJECT_DIR}/include/)

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

add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_LINKER} -m elf_i386 -T ${CMAKE_SOURCE_DIR}/kernel/link.ld
          -o ${KERNEL}
          --start-group

          --whole-archive
          $<TARGET_FILE:asm_objs>
          $<TARGET_FILE:cpp_objs>

          --no-whole-archive
          $<TARGET_FILE:kernel_libc>

          --end-group

  COMMAND grub-file --is-x86-multiboot ${KERNEL}

  COMMENT "Link ${KERNEL}"
  DEPENDS asm_objs cpp_objs kernel_libc
  VERBATIM)
