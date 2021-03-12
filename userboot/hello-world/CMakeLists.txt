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

enable_language(C)
set(CMAKE_C_STANDARD 11)

add_executable(hello-world hello.c)

target_include_directories(hello-world
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
  PRIVATE ${LIBC_PROJECT_DIR}/include
  PRIVATE ${LIBCXX_PROJECT_DIR}/include
  PRIVATE ${UTILS_PROJECT_DIR}/include)

target_compile_options(hello-world
  PRIVATE -Wall -Werror
  PRIVATE -fno-PIC

  # Note this is only needed for clang. If we're using GCC or another compiler,
  # this flag may not be needed.
  PRIVATE -target i386)

target_link_options(hello-world
  PRIVATE -target i386
  PRIVATE -nostdlib
  PRIVATE -fuse-ld=lld
  PRIVATE -Wl,-no-pie
  PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/../user_elf_link.ld)

target_link_libraries(hello-world
  PRIVATE -Wl,--start-group
          user_libc
          user_utils
          -Wl,--end-group)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/hello-world)
