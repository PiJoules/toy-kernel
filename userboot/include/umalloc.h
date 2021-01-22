#ifndef UMALLOC_H_
#define UMALLOC_H_

namespace user {

void *umalloc(size_t size);
void *umalloc(size_t size, uint32_t alignment);
void ufree(void *ptr);
void *urealloc(void *ptr, size_t size);
void *ucalloc(size_t num, size_t size);

void InitializeUserHeap();

}  // namespace user

#endif
