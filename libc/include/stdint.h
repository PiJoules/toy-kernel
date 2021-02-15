#ifndef STDINT_H_
#define STDINT_H_

#include <_internals.h>

__BEGIN_CDECLS

typedef __SIZE_TYPE__ size_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
_Static_assert(sizeof(uint8_t) == 1, "");
_Static_assert(sizeof(uint16_t) == 2, "");
_Static_assert(sizeof(uint32_t) == 4, "");
_Static_assert(sizeof(uint64_t) == 8, "");

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
_Static_assert(sizeof(int8_t) == 1, "");
_Static_assert(sizeof(int16_t) == 2, "");
_Static_assert(sizeof(int32_t) == 4, "");
_Static_assert(sizeof(int64_t) == 8, "");

typedef uint32_t uint_least32_t;

_Static_assert(sizeof(uint_least32_t) >= 4, "");

typedef uint32_t uintptr_t;

_Static_assert(sizeof(uintptr_t) >= sizeof(char *), "");

#define UINT8_C(x) (static_cast<uint8_t>(x))
#define UINT16_C(x) (static_cast<uint16_t>(x))
#define UINT32_C(x) (static_cast<uint32_t>(x))
#define UINT64_C(x) (static_cast<uint64_t>(x))

#define UINT8_MAX (255)
#define UINT16_MAX (65535)
#define UINT32_MAX (4294967295U)
#define UINT64_MAX (18446744073709551615ULL)

#define INT32_MIN (-2147483648)
#define INT32_MAX 0x7fffffff

// Number of bits in a byte.
#define CHAR_BIT 8

__END_CDECLS

#endif
