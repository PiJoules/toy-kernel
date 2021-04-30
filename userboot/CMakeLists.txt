# This contains the build rules for the optional default userboot program
# which either runs tests or launches a basic shell. This is NOT a part of the
# kernel. This is just an example of a user program the kernel first launches
# after booting up. All other features that need to be loaded can be done
# through some custom user program that can replace this.
#
# This is all done through QEMU without graphics and communicated over serial.
#
# CMake code in here should not be dependent on any cmake in the parent directory.
#
# CMake Arguments:
#
# - INITRD_DST_DIR (String):
#     If INITRD_DST_DIR is defined either in a parent project or during the cmake
#     invocation, we will move initrd here once it's finished building.

cmake_minimum_required(VERSION 3.15.4)
project(toy-kernel-userboot)
enable_language(CXX ASM)
set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED INITRD)
  set(INITRD initrd)
endif()

if(DEFINED INITRD_DST_DIR)
  set(SHOULD_COPY_INITRD true)
else()
  set(SHOULD_COPY_INITRD false)
endif()

# (Minimum) Flags needed for compiling C source files to object files.
set(USER_C_FLAGS
  -fno-PIC)

# (Minimum) Flags needed for compiling CXX source files to object files.
set(USER_CXX_FLAGS
  -fno-PIC)

# (Minimum) Flags needed for linking object files to a final executable.
set(USER_LINK_FLAGS
  # Do not use any system default library. This will manually be linked in.
  -nostdlib

  # We do not yet support PIC.
  -Wl,-no-pie

  # We will need to place specific sections at specific addresses.
  -T ${CMAKE_CURRENT_SOURCE_DIR}/user_elf_link.ld)

# Flags used for user programs within this repo.
set(USER_COMPILE_FLAGS
  -fno-stack-protector
  -g
  -fno-exceptions
  -fno-rtti
  -fdata-sections
  -ffunction-sections
  -ftrivial-auto-var-init=pattern
  ${USER_CXX_FLAGS})

# Flags used for user programs within this repo.
set(USER_ELF_LINK_FLAGS
  -Wl,--gc-sections
  -Wl,--fatal-warnings
  ${USER_LINK_FLAGS})

set(USER_PROGRAMS "" CACHE INTERNAL "")

function(add_to_initrd binary)
  get_filename_component(path ${binary} REALPATH)
  set(USER_PROGRAMS "${USER_PROGRAMS} ${path}" CACHE INTERNAL "")
  message(STATUS "Add ${binary} to USER_PROGRAMS")
endfunction()

# Attempt to find a target library called "c" that represents a static archive
# libc implementation.
# FIXME: We should not assume the directory is adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)
set(BUILD_USER_LIBC 1)
add_subdirectory(${LIBC_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_libc_build)

# FIXME: We should not assume the directory is adjascent to this one.
set(LIBCXX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcxx)
set(BUILD_USER_LIBCXX 1)
add_subdirectory(${LIBCXX_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_libcxx_build)

# FIXME: We should not assume the directory is adjascent to this one.
set(UTILS_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils)
set(BUILD_USER_UTILS 1)
add_subdirectory(${UTILS_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_utils_build)

add_library(bootstrap_lib STATIC start_user_program.S)

# All source files are passed via ARGN.
function(make_flat_user_binary name)
  set(libname "${name}_lib")
  add_library(${libname} STATIC "${ARGN}")
  target_compile_options(${libname}
    PRIVATE ${USER_COMPILE_FLAGS})
  target_include_directories(${libname} PRIVATE include/)
  target_include_directories(${libname} PRIVATE ${LIBC_PROJECT_DIR}/include/)
  target_include_directories(${libname} PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
  target_include_directories(${libname} PRIVATE ${UTILS_PROJECT_DIR}/include/)

  add_custom_target(${name} ALL
    COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -m elf_i386
            -o ${name}
            --gc-sections
            --start-group

            --whole-archive
            $<TARGET_FILE:bootstrap_lib>

            --no-whole-archive
            $<TARGET_FILE:${libname}>
            $<TARGET_FILE:user_libc>
            $<TARGET_FILE:user_libcxx>
            $<TARGET_FILE:user_utils>

            --end-group

            COMMENT "Linking ${name}"
    DEPENDS bootstrap_lib ${libname} user_libc user_libcxx
    VERBATIM)
endfunction()

# Test for checking that user programs have different address spaces.
make_flat_user_binary(test_user_program.bin test_user_program.c)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/test_user_program.bin")

# Test for checking that we don't have direct access to I/O instructions in usermode.
make_flat_user_binary(test_user_io_privilege test_user_io_privilege.cpp)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/test_user_io_privilege")

# Userboot stage 1. The sole purpose of this is to run Userboot stage 2,
# which contains the rest of the userboot logic, but in ELF format.
make_flat_user_binary(userboot userboot.cpp elf.cpp)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userboot")

add_subdirectory(sdk)

# Userboot stage 2. This continues userboot, but we can run elf binaries,
# giving us better debugability and something closer to a binary we could
# normally compile on a linux shell without too many special flags.
add_executable(userboot-stage2.debug userboot-stage2.cpp elf.cpp)
target_compile_options(userboot-stage2.debug PRIVATE ${USER_COMPILE_FLAGS})
target_link_options(userboot-stage2.debug PRIVATE ${USER_ELF_LINK_FLAGS})
target_link_libraries(userboot-stage2.debug sdk_cxx_static)
target_include_directories(userboot-stage2.debug PRIVATE include/)
target_include_directories(userboot-stage2.debug PRIVATE ${LIBC_PROJECT_DIR}/include/)
target_include_directories(userboot-stage2.debug PRIVATE ${LIBCXX_PROJECT_DIR}/include/)
target_include_directories(userboot-stage2.debug PRIVATE ${UTILS_PROJECT_DIR}/include/)

add_custom_target(userboot-stage2 ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all userboot-stage2.debug userboot-stage2
  COMMENT "Stripping userboot-stage2.debug"
  DEPENDS userboot-stage2.debug
  VERBATIM)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userboot-stage2")

add_subdirectory(userspace-tests)
add_subdirectory(hello-world)
add_subdirectory(shell)

set(USER_PROGRAMS_LIST ${USER_PROGRAMS})
separate_arguments(USER_PROGRAMS_LIST)

find_package(Python3 COMPONENTS Interpreter)
if (NOT ${Python3_Interpreter_FOUND})
  message(FATAL_ERROR "Could not find python3 interpreter")
endif()

set(INITRD_DIR initrd_files/)

add_custom_target(${INITRD}
  COMMAND mkdir -p ${INITRD_DIR}

  # Copy user programs + data
  COMMAND echo "USER_PROGRAMS: ${USER_PROGRAMS}"
  COMMAND /bin/sh -c "for f in ${USER_PROGRAMS}; do cp $f ${INITRD_DIR} ; done"
  COMMAND tar --format=ustar -cf ${INITRD}.tar ${INITRD_DIR}

  # The initrd will consist of the concatenation of the entry program at the
  # start of the file and the tarball for the ustar filesystem.
  COMMAND /bin/sh -c "cat userboot ${INITRD}.tar > ${INITRD}"

  COMMAND /bin/sh -c "if ${SHOULD_COPY_INITRD}; then cp ${INITRD} ${INITRD_DST_DIR}/; fi"

  COMMENT "Create the initial ramdisk with '${USER_PROGRAMS}'"
  DEPENDS ${USER_PROGRAMS_LIST}
  VERBATIM)
