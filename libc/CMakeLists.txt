# CMake build rules for making a libc compatible with this kernel.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-libc)
enable_language(CXX ASM)
set(CMAKE_CXX_STANDARD 17)

set(COMMON_SRCS
  assert.cpp
  ctype.cpp
  stdlib.cpp
  stacktrace.cpp
  string.cpp
  type.cpp)

set(KERNEL_SRCS "")

set(USER_SRCS
  crt0.S
  umalloc.cpp
  pre-main.cpp
  printf.cpp
  elf.cpp
  opendir.cpp
  getcwd.cpp
  system.cpp
  _syscalls.cpp)

# FIXME: We should not assume the directory is adjascent to this one.
set(UTILS_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils)

# FIXME: We should not assume the directory is adjascent to this one.
# We should also not try to depend on the adjascent libcxx directory since
# this results in a circular dependency where libc depends on utils (for
# printing), which depends on libcxx (for type_traits), which depends on libc.
set(LIBCXX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcxx)

if (DEFINED BUILD_USER_LIBC AND NOT TARGET user_libc)
  add_library(user_libc_nopic STATIC ${USER_SRCS} ${COMMON_SRCS})
  target_compile_options(user_libc_nopic
    PRIVATE -fno-PIC)

  # FIXME: Uncomment this once we support dynamically linking + loading shared
  # libs.
  #add_library(user_libc_shared SHARED ${USER_SRCS} ${COMMON_SRCS})

  add_library(user_libc_pic_static STATIC ${USER_SRCS} ${COMMON_SRCS})
endif()

if (DEFINED BUILD_KERNEL_LIBC AND NOT TARGET kernel_libc)
  add_library(kernel_libc STATIC ${KERNEL_SRCS} ${COMMON_SRCS})
  target_include_directories(kernel_libc PRIVATE include/)
  target_include_directories(kernel_libc PRIVATE ${UTILS_PROJECT_DIR}/include/)
  target_include_directories(kernel_libc PRIVATE ${LIBCXX_PROJECT_DIR}/include/)

  # FIXME: We should not assume the directory is adjascent to this one.
  set(KERNEL_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kernel)
  target_include_directories(kernel_libc PRIVATE ${KERNEL_PROJECT_DIR}/include/)

  target_compile_options(kernel_libc
    PRIVATE ${KERNEL_COMPILE_FLAGS})
  target_compile_definitions(kernel_libc PRIVATE KERNEL)
endif()
