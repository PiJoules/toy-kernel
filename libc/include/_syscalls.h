#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdbool.h>
#include <stdint.h>

__BEGIN_CDECLS

bool sys_debug_read(char *c);
int32_t sys_debug_print(const char *str);
bool sys_debug_put(char);
void sys_exit_task();

typedef uint32_t Handle;
#define HANDLE_INVALID 0
Handle sys_create_task(const void *entry, uint32_t codesize, void *arg,
                       size_t entry_offset);
void sys_destroy_task(Handle handle);
void sys_copy_from_task(Handle handle, void *dst, const void *src, size_t size);
Handle sys_get_parent_task();
uint32_t sys_get_parent_task_id();

#define MAP_SUCCESS (0)
#define MAP_UNALIGNED_ADDR (-1)
#define MAP_ALREADY_MAPPED (-2)
#define MAP_OOM (-3)
int32_t sys_map_page(void *addr);

__END_CDECLS

// Provide a nice C++ API if available.
#ifdef __cplusplus
namespace sys {

inline bool DebugRead(char &c) { return sys_debug_read(&c); }
inline int32_t DebugPrint(const char *str) { return sys_debug_print(str); }
inline bool DebugPut(char c) { return sys_debug_put(c); }
inline void ExitTask() { return sys_exit_task(); }

using Handle = ::Handle;
inline Handle CreateTask(const void *entry, uint32_t codesize,
                         void *arg = nullptr, size_t entry_offset = 0) {
  return sys_create_task(entry, codesize, arg, entry_offset);
}
inline void DestroyTask(Handle handle) { return sys_destroy_task(handle); }

}  // namespace sys
#endif

#endif
