#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdint.h>

namespace sys {

bool DebugRead(char &c);
uint32_t DebugPrint(const char *str);
void DebugPut(char);
void ExitTask();

}  // namespace sys

#endif
