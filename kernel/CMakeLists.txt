# This contains the build rules for making the kernel.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-kernel)
enable_language(CXX)

add_library(asm_objs STATIC boot.S gdt.S interrupt.S task.S)
target_compile_options(asm_objs
  PRIVATE -target i386-elf)

set(KERNEL_COMPILE_FLAGS
  -target i386
  -fno-PIC
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
  -fno-omit-frame-pointer)

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

add_executable(${KERNEL}.debug
  descriptortables.cpp
  isr.cpp
  kernel.cpp
  kmalloc.cpp
  paging.cpp
  panic.cpp
  serial.cpp
  syscall.cpp
  task.cpp
  tests.cpp
  timer.cpp)
target_compile_options(${KERNEL}.debug PRIVATE ${KERNEL_COMPILE_FLAGS})
target_include_directories(${KERNEL}.debug PRIVATE include/)
target_include_directories(${KERNEL}.debug PRIVATE ${LIBC_PROJECT_DIR}/include/)
target_include_directories(${KERNEL}.debug PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
target_include_directories(${KERNEL}.debug PRIVATE ${UTILS_PROJECT_DIR}/include/)
target_compile_definitions(${KERNEL}.debug PRIVATE KERNEL)
target_link_options(${KERNEL}.debug
  PRIVATE -target i386-elf
  PRIVATE -fuse-ld=lld
  PRIVATE -T ${CMAKE_SOURCE_DIR}/kernel/link.ld
  PRIVATE -nostdlib
  PRIVATE -Wl,-no-pie
  PRIVATE -Wl,--gc-sections)
target_link_libraries(${KERNEL}.debug
  -Wl,--start-group
  -Wl,--no-whole-archive
  asm_objs
  kernel_libc
  kernel_libcxx
  kernel_utils
  -Wl,--end-group)


# Creates the stripped version without debuginfo.
add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all ${KERNEL}.debug ${KERNEL}
  COMMAND grub-file --is-x86-multiboot ${KERNEL}

  COMMENT "Stripping ${KERNEL}.debug"
  DEPENDS ${KERNEL}.debug
  VERBATIM)
