# This directory contains all the necessary targets and headers for the SDK
# used for building for and interacting with this OS.
#
# The SDK includes all the libraries necessary for building a program that can
# run on this OS.
#
# Available targets:
#
#   sdk_c_static: Static library containing libc definitions.
#   sdk_cxx_static: Static library containing libc++ definitions.
#
# The *_nopic equivalents provide libraries that use absolute addressing modes.

# Create an empty dummy file so cmake will not complain about a library with no
# sources.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")

add_library(sdk_c_pic_static STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_c_pic_static
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc_pic_static user_libcxx_pic_static user_utils_pic_static
  -Wl,--end-group)

add_library(sdk_cxx_static STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_cxx_static
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc user_libcxx user_utils
  -Wl,--end-group)

add_library(sdk_cxx_pic_static STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_cxx_pic_static
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc_pic_static user_libcxx_pic_static user_utils_pic_static
  -Wl,--end-group)

add_library(sdk_cxx_static_nopic STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_cxx_static_nopic
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc_nopic user_libcxx_nopic user_utils_nopic
  -Wl,--end-group)
