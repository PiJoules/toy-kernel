  .section .text
call main

// Call user_thread_exit().
mov $1, %eax
int $0x80
jmp .
