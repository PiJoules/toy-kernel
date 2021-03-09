#ifndef UMALLOC_H_
#define UMALLOC_H_

#ifdef __cplusplus
namespace user {

void *umalloc(size_t size);
void *umalloc(size_t size, uint32_t alignment);
void ufree(void *ptr);
void *urealloc(void *ptr, size_t size);
void *ucalloc(size_t num, size_t size);

void InitializeUserHeap(uint8_t *heap_bottom, uint8_t *heap_top);

}  // namespace user
#else
void InitializeUserHeap(uint8_t *heap_bottom, uint8_t *heap_top);
#endif

#endif
