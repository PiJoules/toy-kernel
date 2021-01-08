#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdint.h>

namespace sys {

uint32_t DebugPrint(const char *str);
uint32_t DebugPut(char c);

}  // namespace sys

#endif
