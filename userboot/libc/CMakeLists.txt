# CMake build rules for making a libc compatible with this kernel.

add_library(c STATIC
  string.cpp
  printf.cpp
  _syscalls.cpp)
target_include_directories(c PRIVATE include/)
target_compile_options(c
  PRIVATE ${USER_COMPILE_FLAGS})
