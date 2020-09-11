add_custom_target(${USER_PROGRAM} ALL
  COMMAND ${CMAKE_C_COMPILER} -target i386 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -o ${USER_PROGRAM}.o -c ${CMAKE_CURRENT_SOURCE_DIR}/user_program.c -g -ftrivial-auto-var-init=pattern
  COMMAND ${CMAKE_C_COMPILER} -target i386 ${CMAKE_CURRENT_SOURCE_DIR}/start_user_program.s -o start_user_program.o -c
  COMMAND ${CMAKE_LINKER} -T ${CMAKE_CURRENT_SOURCE_DIR}/user_link.ld -melf_i386 -o ${USER_PROGRAM} start_user_program.o ${USER_PROGRAM}.o

  # TODO: Support position independent code. We fail during the linking step with
  # "undefined reference to `_GLOBAL_OFFSET_TABLE_'".
  #COMMAND ${CMAKE_C_COMPILER} -Wl,-T${CMAKE_SOURCE_DIR}/user_link.ld -o ${USER_PROGRAM} ${CMAKE_SOURCE_DIR}/start_user_program.s ${CMAKE_SOURCE_DIR}/user_program.c -target i386 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -g -fPIE
  COMMENT Compiling ${USER_PROGRAM}
  VERBATIM)
