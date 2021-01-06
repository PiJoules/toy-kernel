# This contains the build rules for the optional default userboot program
# which either runs tests or launches a basic shell. This is NOT a part of the
# kernel. This is just an example of a user program the kernel first launches
# after booting up. All other features that need to be loaded can be done
# through some custom user program that can replace this.
#
# This is all done through QEMU without graphics and communicated over serial.
#
# CMake code in here should not be dependent on any cmake in the parent directory.

set(INITRD initrd.vfs)

set(USER_COMPILE_FLAGS
  -target i386
  -nostdlib
  -nostdinc
  -fno-builtin
  -fno-stack-protector
  -nostartfiles
  -nodefaultlibs
  -g
  -ftrivial-auto-var-init=pattern)

set(USER_PROGRAMS "" CACHE INTERNAL "")

function(add_to_initrd binary)
  get_filename_component(path ${binary} REALPATH)
  set(USER_PROGRAMS "${USER_PROGRAMS} ${path}" CACHE INTERNAL "")
  message(STATUS "Add ${binary} to USER_PROGRAMS (${USER_PROGRAMS})")
endfunction()

add_library(bootstrap_lib STATIC start_user_program.s)
target_compile_options(bootstrap_lib
  PRIVATE -target i386)

# All source files are passed via ARGN.
function(make_flat_user_binary name)
  set(libname "${name}_lib")
  add_library(${libname} STATIC "${ARGN}")
  target_compile_options(${libname}
    PRIVATE ${USER_COMPILE_FLAGS})

  add_custom_target(${name} ALL
    COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -m elf_i386
            -o ${name}
            --whole-archive
            --start-group
            $<TARGET_FILE:${libname}>
            $<TARGET_FILE:bootstrap_lib>
            --end-group

    COMMENT "Compiling ${name}"
    DEPENDS bootstrap_lib ${libname}
    VERBATIM)
endfunction()

make_flat_user_binary(test_user_program.bin test_user_program.c)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/test_user_program.bin")

make_flat_user_binary(userboot userboot.cpp)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userboot")

set(USER_PROGRAMS_LIST ${USER_PROGRAMS})
separate_arguments(USER_PROGRAMS_LIST)

find_package(Python3 COMPONENTS Interpreter)
if (NOT ${Python3_Interpreter_FOUND})
  message(FATAL_ERROR "Could not find python3 interpreter")
endif()

add_custom_target(${INITRD}
  COMMAND mkdir -p initrd/

  # Copy user programs + data
  COMMAND echo "USER_PROGRAMS: ${USER_PROGRAMS}"
  COMMAND /bin/sh -c "for f in ${USER_PROGRAMS}; do cp $f initrd/ ; done"
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/initrd.py -o initrd.vfs initrd/
  COMMAND cp initrd.vfs ${PROJECT_BINARY_DIR}/

  COMMENT "Create the initial ramdisk with '${USER_PROGRAMS}'"
  DEPENDS ${USER_PROGRAMS_LIST}
  VERBATIM)
