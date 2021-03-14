# This contains the bare minimum flags/tunings necessary for building an
# executable that can run in this environment. The goal of this subproject is
# to make it such that writing programs for this environment is simple.
#
# This also should help provide a basis for how to build programs out-of-tree.
#
# There are a number of FIXMEs that should be addressed before this is made
# "simple":
#
# - Any program meant to be built on this platform should only need to link
#   against these binaries:
#   - A user libc (compatible with this environment)
#     - Potentially a libc++, but one step at a time
#   - A userspace SDK
#     - This will contain the API for interacting with this platform. This
#       includes any necessary headers and function implementations.
# - The only linker flag necessary should be `-nostdlib` so that the host
#   compiler doesn't use any default libc.
#
# If this ever gets compiler support, hopefully at some point, we can just do
# something like:
#
#   $ clang -target i386-this-os hello.c -o hello
#   $ cp hello path/to/initrd  # Then create the initial ramdisk with this.
#
# FIXME: It may be likely that for some systems, the compiler driver may not
# include the proper paths that contain C headers for i386. In this case, we
# should add them manually to the target.

enable_language(C)
set(CMAKE_C_STANDARD 11)

add_executable(hello-world hello.c)

target_compile_options(hello-world
  # We do not yet support PIC.
  PRIVATE -fno-PIC)

target_link_options(hello-world
  # Do not use any system default library. This will manually be linked in.
  PRIVATE -nostdlib

  # We do not yet support PIC.
  PRIVATE -Wl,-no-pie

  # We will need to place specific sections at specific addresses.
  PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/../user_elf_link.ld)

target_link_libraries(hello-world
  # Link in the necessary files.
  PRIVATE -Wl,--start-group
          -Wl,--no-whole-archive
          user_libc
          user_utils
          -Wl,--end-group)

# Add to the initial ramdisk.
add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/hello-world)