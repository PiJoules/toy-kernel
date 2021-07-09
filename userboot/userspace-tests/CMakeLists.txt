# This contains tests for various userspace programs.

add_custom_target(userspace-tests ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all userspace-tests.debug userspace-tests
  COMMENT "Stripping userspace-tests.debug"
  DEPENDS userspace-tests.debug
  VERBATIM)

add_executable(userspace-tests.debug userspace-tests.cpp)
target_link_libraries(userspace-tests.debug sdk_cxx_pic_static)
target_include_directories(userspace-tests.debug
  PRIVATE include/
  PRIVATE ../include/)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userspace-tests"
              "tests/userspace-tests")
