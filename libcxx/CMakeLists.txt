# CMake build rules for making a libc++ compatible with this kernel (with
# some limited features).

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-libcxx)
enable_language(CXX ASM)
set(CMAKE_CXX_STANDARD 17)

set(COMMON_SRCS
  printf.cpp
  string.cpp
  type.cpp)

set(KERNEL_SRCS "")

set(USER_SRCS
  _syscalls.cpp)

if (DEFINED BUILD_USER_LIBC)
  add_library(user_libc STATIC ${USER_SRCS} ${COMMON_SRCS})
  target_include_directories(user_libc PRIVATE include/)
  target_compile_options(user_libc
    PRIVATE ${USER_COMPILE_FLAGS})
endif()

if (DEFINED BUILD_KERNEL_LIBC)
  add_library(kernel_libc STATIC ${KERNEL_SRCS} ${COMMON_SRCS})
  target_include_directories(kernel_libc PRIVATE include/)
  target_compile_options(kernel_libc
    PRIVATE ${KERNEL_COMPILE_FLAGS})
  target_compile_definitions(kernel_libc PRIVATE KERNEL)
endif()
