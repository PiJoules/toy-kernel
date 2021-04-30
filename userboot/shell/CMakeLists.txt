# This directory contains source code for a basic shell.

add_executable(shell shell.cpp)
target_compile_options(shell PRIVATE ${USER_COMPILE_FLAGS})
target_link_options(shell PRIVATE ${USER_LINK_FLAGS})
target_link_libraries(shell sdk_cxx_static)
target_include_directories(shell
  PRIVATE ${LIBC_PROJECT_DIR}/include
  PRIVATE ${LIBCXX_PROJECT_DIR}/include
  PRIVATE ${UTILS_PROJECT_DIR}/include)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/shell")

#add_executable(ls ls.cpp)
#target_compile_options(ls PRIVATE ${USER_COMPILE_FLAGS})
#target_link_options(ls PRIVATE ${USER_LINK_FLAGS})
#target_link_libraries(ls sdk_cxx_static)
#target_include_directories(ls
#  PRIVATE ${LIBC_PROJECT_DIR}/include
#  PRIVATE ${LIBCXX_PROJECT_DIR}/include
#  PRIVATE ${UTILS_PROJECT_DIR}/include)
#
#add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/ls")
