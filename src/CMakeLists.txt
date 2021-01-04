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
