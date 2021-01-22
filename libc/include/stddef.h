#ifndef STDDEF_H_
#define STDDEF_H_

#define offsetof(Type, Member) __builtin_offsetof(Type, Member)

// FIXME: We should instead define max_align_t.
constexpr unsigned kMaxAlignment = 4;

#endif
