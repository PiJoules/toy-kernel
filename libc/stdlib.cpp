#include <stdlib.h>

#ifdef KERNEL
#include <kmalloc.h>
#include <panic.h>

void abort() { PANIC("abort"); }

void *malloc(size_t size) { return kmalloc(size); }

void *malloc(size_t size, uint32_t alignment) {
  return kmalloc(size, alignment);
}

void free(void *ptr) { return kfree(ptr); }

void *realloc(void *ptr, size_t new_size) { return krealloc(ptr, new_size); }

void *calloc(size_t num, size_t size) { return kcalloc(num, size); }
#else

namespace user {
extern void *umalloc(size_t size);
extern void *umalloc(size_t size, uint32_t alignment);
extern void ufree(void *ptr);
extern void *urealloc(void *ptr, size_t size);
extern void *ucalloc(size_t num, size_t size);
}  // namespace user

void *malloc(size_t size) { return user::umalloc(size); }

void *aligned_alloc(size_t alignment, size_t size) {
  return user::umalloc(size, alignment);
}

void free(void *ptr) { return user::ufree(ptr); }

void *realloc(void *ptr, size_t new_size) {
  return user::urealloc(ptr, new_size);
}

void *calloc(size_t num, size_t size) { return user::ucalloc(num, size); }

#endif
