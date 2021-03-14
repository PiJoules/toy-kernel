# This contains tests for various userspace programs.

add_custom_target(userspace-tests ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all userspace-tests.debug userspace-tests
  COMMENT "Stripping userspace-tests.debug"
  DEPENDS userspace-tests.debug
  VERBATIM)

add_executable(userspace-tests.debug userspace-tests.cpp)
target_compile_options(userspace-tests.debug
  PRIVATE ${USER_COMPILE_FLAGS})
target_link_options(userspace-tests.debug
  PRIVATE ${USER_ELF_LINK_FLAGS})
target_link_libraries(userspace-tests.debug sdk_cxx_static)
target_include_directories(userspace-tests.debug
  PRIVATE include/
  PRIVATE ../include/  # userboot includes
  PRIVATE ${LIBC_PROJECT_DIR}/include/
  PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
target_include_directories(userspace-tests.debug PRIVATE ${UTILS_PROJECT_DIR}/include/)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userspace-tests")
