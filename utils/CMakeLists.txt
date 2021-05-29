# CMake build rules for making utilities that can be used in this project.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-util)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

set(COMMON_SRCS
  allocator.cpp
  builtins.cpp
  print.cpp)

set(KERNEL_SRCS)

set(USER_SRCS
  vfs.cpp)

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)
set(LIBCXX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcxx)

if (DEFINED BUILD_KERNEL_UTILS)
  add_library(kernel_utils STATIC ${COMMON_SRCS} ${KERNEL_SRCS})
  target_include_directories(kernel_utils PRIVATE include/)
  target_include_directories(kernel_utils PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(kernel_utils PRIVATE ${LIBCXX_PROJECT_DIR}/include/)

  target_compile_options(kernel_utils
    PRIVATE ${KERNEL_COMPILE_FLAGS})
  target_compile_definitions(kernel_utils PRIVATE KERNEL)
endif()

if (DEFINED BUILD_USER_UTILS)
  add_library(user_utils STATIC ${COMMON_SRCS} ${USER_SRCS})
  target_include_directories(user_utils PRIVATE include/)
  target_include_directories(user_utils PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(user_utils PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
  target_compile_options(user_utils PRIVATE ${USER_COMPILE_FLAGS})
  target_link_libraries(user_utils user_libcxx)

  add_library(user_utils_shared SHARED ${COMMON_SRCS} ${USER_SRCS})
  target_include_directories(user_utils_shared PRIVATE include/)
  target_include_directories(user_utils_shared PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(user_utils_shared PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
  target_compile_options(user_utils_shared
    PRIVATE ${USER_COMPILE_FLAGS}
    PRIVATE -fPIC)

  add_library(user_utils_pic STATIC ${COMMON_SRCS} ${USER_SRCS})
  target_include_directories(user_utils_pic PRIVATE include/)
  target_include_directories(user_utils_pic PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(user_utils_pic PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
  target_compile_options(user_utils_pic
    PRIVATE ${USER_COMPILE_FLAGS}
    PRIVATE -fPIC)
  target_link_libraries(user_utils_pic user_libcxx)
endif()
