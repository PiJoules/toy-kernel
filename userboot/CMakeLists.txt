# This contains the build rules for the optional default userboot program
# which either runs tests or launches a basic shell. This is NOT a part of the
# kernel. This is just an example of a user program the kernel first launches
# after booting up. All other features that need to be loaded can be done
# through some custom user program that can replace this.
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

# Flags used for user programs within this repo.
set(USER_COMPILE_FLAGS
  -fvisibility=hidden
  -fno-stack-protector
  -g
  -fdata-sections
  -ffunction-sections
  -ftrivial-auto-var-init=pattern
  -nostdinc  # Do not use system headers.
  -fPIC)

# (Minimum) Flags needed for compiling C source files to object files.
set(USER_C_FLAGS
  ${USER_COMPILE_FLAGS})

# (Minimum) Flags needed for compiling CXX source files to object files.
set(USER_CXX_FLAGS
  ${USER_COMPILE_FLAGS}
  -fno-exceptions
  -fno-rtti)

add_compile_options(
  "$<$<COMPILE_LANGUAGE:C>:${USER_C_FLAGS}>"
  "$<$<COMPILE_LANGUAGE:CXX>:${USER_CXX_FLAGS}>"
)

# (Minimum) Flags needed for linking object files to a final executable.
set(USER_LINK_FLAGS
  # Do not use any system default library. This will manually be linked in.
  -nostdlib)

# Flags used for user programs within this repo.
set(USER_ELF_LINK_FLAGS
  ${USER_LINK_FLAGS}
  -Wl,--gc-sections
  -Wl,--fatal-warnings
  -fPIE)

add_link_options(${USER_ELF_LINK_FLAGS})

set(USER_PROGRAMS "" CACHE INTERNAL "")
set(USER_PROGRAM_DSTS "" CACHE INTERNAL "")

function(add_to_initrd binary dst)
  get_filename_component(path ${binary} REALPATH)
  set(USER_PROGRAMS "${USER_PROGRAMS} ${path}" CACHE INTERNAL "")
  set(USER_PROGRAM_DSTS "${USER_PROGRAM_DSTS} ${dst}" CACHE INTERNAL "")
  message(STATUS "Added ${binary} to USER_PROGRAMS")
endfunction()

# Attempt to find a target library called "c" that represents a static archive
# libc implementation.
# FIXME: We should not assume these directories are adjascent to this one.
set(LIBC_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libc)
set(LIBCXX_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcxx)
set(UTILS_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils)

include_directories(
  ${LIBC_PROJECT_DIR}/include
  ${LIBCXX_PROJECT_DIR}/include
  ${UTILS_PROJECT_DIR}/include)

set(BUILD_USER_LIBC 1)
add_subdirectory(${LIBC_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_libc_build)
set(BUILD_USER_LIBCXX 1)
add_subdirectory(${LIBCXX_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_libcxx_build)
set(BUILD_USER_UTILS 1)
add_subdirectory(${UTILS_PROJECT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/user_utils_build)

add_library(bootstrap_lib STATIC start_user_program.S)

# All source files are passed via ARGN.
function(make_flat_user_binary name)
  set(libname "${name}_lib")
  add_library(${libname} STATIC "${ARGN}")
  target_compile_options(${libname}
    PRIVATE ${USER_CXX_FLAGS}
    PRIVATE -fno-PIC)
  target_include_directories(${libname} PRIVATE include/)

  add_custom_target(${name} ALL
    COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -m elf_i386
            -o ${name}
            --gc-sections
            --start-group

            --whole-archive
            $<TARGET_FILE:bootstrap_lib>

            --no-whole-archive
            $<TARGET_FILE:${libname}>
            $<TARGET_FILE:user_libc_nopic>
            $<TARGET_FILE:user_libcxx_nopic>
            $<TARGET_FILE:user_utils_nopic>

            --end-group

            COMMENT "Linking ${name}"
    DEPENDS bootstrap_lib ${libname} user_libc_nopic user_libcxx_nopic user_utils_nopic
    VERBATIM)
endfunction()

# Test for checking that user programs have different address spaces.
make_flat_user_binary(test_user_program.bin test_user_program.c)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/test_user_program.bin"
              "test_user_program.bin")

# Test for checking that we don't have direct access to I/O instructions in usermode.
make_flat_user_binary(test_user_io_privilege test_user_io_privilege.cpp)
add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/test_user_io_privilege"
              "test_user_io_privilege")

# Userboot stage 1. The sole purpose of this is to run Userboot stage 2,
# which contains the rest of the userboot logic, but in ELF format.
make_flat_user_binary(userboot userboot.cpp)

add_subdirectory(sdk)

# Userboot stage 2. This continues userboot, but we can run elf binaries,
# giving us better debugability and something closer to a binary we could
# normally compile on a linux shell without too many special flags.
add_executable(userboot-stage2.debug userboot-stage2.cpp)
target_link_libraries(userboot-stage2.debug sdk_cxx_pic_static)
target_include_directories(userboot-stage2.debug PRIVATE include/)

add_custom_target(userboot-stage2 ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all userboot-stage2.debug userboot-stage2
  COMMENT "Stripping userboot-stage2.debug"
  DEPENDS userboot-stage2.debug
  VERBATIM)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/userboot-stage2"
              "userboot-stage2")

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
  COMMAND python3 ${CMAKE_SOURCE_DIR}/userboot/make_initrd.py
          --files ${USER_PROGRAMS} --dests ${USER_PROGRAM_DSTS}
          --initrd-dir ${INITRD_DIR}
  COMMAND tar --format=ustar -cf ${INITRD}.tar -C ${INITRD_DIR} .

  # The initrd will consist of the concatenation of the entry program at the
  # start of the file and the tarball for the ustar filesystem.
  COMMAND /bin/sh -c "cat userboot ${INITRD}.tar > ${INITRD}"

  COMMAND /bin/sh -c "if ${SHOULD_COPY_INITRD}; then cp ${INITRD} ${INITRD_DST_DIR}/; fi"

  COMMENT "Create the initial ramdisk with '${USER_PROGRAMS}'"
  DEPENDS userboot ${USER_PROGRAMS_LIST}
  VERBATIM)
