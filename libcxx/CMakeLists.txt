# CMake build rules for making a libc++ compatible with this kernel (with
# some limited features).

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-libcxx)
enable_language(CXX ASM)
set(CMAKE_CXX_STANDARD 17)

set(COMMON_SRCS
  cxa_pure_virtual.cpp
  new.cpp)

set(KERNEL_SRCS "")

set(USER_SRCS "")

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)

# FIXME: We should not assume the directory is adjascent to this one.
set(KERNEL_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kernel)

if (DEFINED BUILD_USER_LIBCXX)
  add_library(user_libcxx STATIC ${USER_SRCS} ${COMMON_SRCS})
  target_include_directories(user_libcxx PRIVATE include/)
  target_compile_options(user_libcxx
    PRIVATE ${USER_COMPILE_FLAGS})

  target_include_directories(user_libcxx PRIVATE ${LIBC_PROJECT_DIR}/include/)
endif()

if (DEFINED BUILD_KERNEL_LIBCXX)
  add_library(kernel_libcxx STATIC ${KERNEL_SRCS} ${COMMON_SRCS})
  target_include_directories(kernel_libcxx PRIVATE include/)
  target_compile_options(kernel_libcxx
    PRIVATE ${KERNEL_COMPILE_FLAGS})
  target_compile_definitions(kernel_libcxx PRIVATE KERNEL)

  target_include_directories(kernel_libcxx PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(kernel_libcxx PRIVATE ${KERNEL_PROJECT_DIR}/include/)
endif()
