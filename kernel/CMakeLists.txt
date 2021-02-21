# This contains the build rules for making the kernel.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-kernel)
enable_language(CXX)

add_library(asm_objs STATIC boot.S gdt.S interrupt.S task.S)
target_compile_options(asm_objs
  PRIVATE -target i386)

add_library(cpp_objs STATIC
  descriptortables.cpp
  isr.cpp
  kernel.cpp
  keyboard.cpp
  kmalloc.cpp
  paging.cpp
  panic.cpp
  serial.cpp
  syscall.cpp
  task.cpp
  tests.cpp
  timer.cpp)
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
  -fdata-sections
  -ffunction-sections
  -UNDEBUG  # We keep all assert()s

  # This is needed for stack tracing.
  -fno-omit-frame-pointer

  -nostdlib)

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)
set(BUILD_KERNEL_LIBC 1)
add_subdirectory(${LIBC_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kernel_libc_build)

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBCXX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcxx)
set(BUILD_KERNEL_LIBCXX 1)
add_subdirectory(${LIBCXX_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kernel_libcxx_build)

# FIXME: We should not assume the directory is adjascent to this one.
set(UTILS_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils)
set(BUILD_KERNEL_UTILS 1)
add_subdirectory(${UTILS_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kernel_utils)

target_compile_options(cpp_objs PRIVATE ${KERNEL_COMPILE_FLAGS})
target_include_directories(cpp_objs PRIVATE ${LIBC_PROJECT_DIR}/include/)
target_include_directories(cpp_objs PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
target_include_directories(cpp_objs PRIVATE ${UTILS_PROJECT_DIR}/include/)

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

# Creates a debug version of the kernel.
add_custom_target(${KERNEL}.debug ALL
  COMMAND ${CMAKE_LINKER} -m elf_i386 -T ${CMAKE_SOURCE_DIR}/kernel/link.ld
          -o ${KERNEL}.debug
          --gc-sections
          --start-group

          --no-whole-archive
          $<TARGET_FILE:asm_objs>
          $<TARGET_FILE:cpp_objs>
          $<TARGET_FILE:kernel_libc>
          $<TARGET_FILE:kernel_libcxx>
          $<TARGET_FILE:kernel_utils>

          --end-group
  COMMENT "Link ${KERNEL}.debug"
  DEPENDS asm_objs cpp_objs kernel_libc kernel_libcxx kernel_utils
  VERBATIM)

# Creates the stripped version without debuginfo.
add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all ${KERNEL}.debug ${KERNEL}
  COMMAND grub-file --is-x86-multiboot ${KERNEL}

  COMMENT "Stripping ${KERNEL}.debug"
  DEPENDS ${KERNEL}.debug
  VERBATIM)
