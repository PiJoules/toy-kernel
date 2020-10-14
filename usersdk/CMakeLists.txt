set(USER_CXX_FLAGS
  -nostdlib
  -nostdinc
  -fno-builtin
  -fno-stack-protector
  -nostartfiles
  -nodefaultlibs
  -g
  -ftrivial-auto-var-init=pattern)

set(USER_PROGRAM user_program.bin)

add_custom_target(${USER_PROGRAM} ALL
  COMMAND ${CMAKE_C_COMPILER} -target i386 -o ${USER_PROGRAM}.o -c ${CMAKE_CURRENT_SOURCE_DIR}/user_program.c ${USER_CXX_FLAGS}
  COMMAND ${CMAKE_C_COMPILER} -target i386 ${CMAKE_CURRENT_SOURCE_DIR}/start_user_program.s -o start_user_program.o -c
  COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -melf_i386 -o ${USER_PROGRAM} start_user_program.o ${USER_PROGRAM}.o

  COMMENT "Compiling ${USER_PROGRAM}"
  VERBATIM)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/${USER_PROGRAM}")

add_custom_target("shell" ALL
  COMMAND ${CMAKE_C_COMPILER} -target i386 -o shell.o -c ${CMAKE_CURRENT_SOURCE_DIR}/user_program.c ${USER_CXX_FLAGS}
  COMMAND ${CMAKE_C_COMPILER} -target i386 ${CMAKE_CURRENT_SOURCE_DIR}/start_user_program.s -o start_user_program.o -c
  COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -melf_i386 -o shell start_user_program.o shell.o

  COMMENT "Compiling shell"
  VERBATIM)

add_to_initrd("${CMAKE_CURRENT_BINARY_DIR}/shell")
