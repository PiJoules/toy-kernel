# This directory contains source code for a basic shell.

add_executable(shell shell.cpp)
target_link_libraries(shell sdk_cxx_pic_static)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/shell"
              "shell")

add_executable(ls ls.cpp)
target_link_libraries(ls sdk_cxx_pic_static)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/ls"
              "bin/ls")
