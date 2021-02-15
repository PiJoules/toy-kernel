#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdbool.h>
#include <stdint.h>

__BEGIN_CDECLS

bool sys_debug_read(char *c);
uint32_t sys_debug_print(const char *str);
void sys_debug_put(char);
void sys_exit_task();

typedef uint32_t Handle;
Handle sys_create_task(const void *entry, uint32_t codesize, void *arg);
void sys_destroy_task(Handle handle);

__END_CDECLS

// Provide a nice C++ API if available.
#ifdef __cplusplus
namespace sys {

inline bool DebugRead(char &c) { return sys_debug_read(&c); }
inline uint32_t DebugPrint(const char *str) { return sys_debug_print(str); }
inline void DebugPut(char c) { return sys_debug_put(c); }
inline void ExitTask() { return sys_exit_task(); }

using Handle = ::Handle;
inline Handle CreateTask(const void *entry, uint32_t codesize,
                         void *arg = nullptr) {
  return sys_create_task(entry, codesize, arg);
}
inline void DestroyTask(Handle handle) { return sys_destroy_task(handle); }

}  // namespace sys
#endif

#endif
