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

# Create an empty dummy file so cmake will not complain about a library with no
# sources.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")
add_library(sdk_c_static STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_c_static
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc user_utils
  -Wl,--end-group)

add_library(sdk_cxx_static STATIC ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
target_link_libraries(sdk_cxx_static
  -Wl,--start-group
  -Wl,--no-whole-archive
  user_libc user_libcxx user_utils
  -Wl,--end-group)
