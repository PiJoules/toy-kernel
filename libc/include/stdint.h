#ifndef STDINT_H_
#define STDINT_H_

typedef __SIZE_TYPE__ size_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
static_assert(sizeof(uint8_t) == 1);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(uint32_t) == 4);
static_assert(sizeof(uint64_t) == 8);

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
static_assert(sizeof(int8_t) == 1);
static_assert(sizeof(int16_t) == 2);
static_assert(sizeof(int32_t) == 4);
static_assert(sizeof(int64_t) == 8);

using uint_least32_t = uint32_t;

static_assert(sizeof(uint_least32_t) >= 4, "");

using uintptr_t = uint32_t;

static_assert(sizeof(uintptr_t) >= sizeof(char *), "");

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

#endif
