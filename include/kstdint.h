#ifndef KSTDINT_H_
#define KSTDINT_H_

// TODO: These should be replaced with a ported libc++.
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = uint32_t;

static_assert(sizeof(uint8_t) == 1, "");
static_assert(sizeof(uint16_t) == 2, "");
static_assert(sizeof(uint32_t) == 4, "");
static_assert(sizeof(uint64_t) == 8, "");

using int8_t = signed char;
using int16_t = signed short;
using int32_t = signed int;
using int64_t = signed long long;

static_assert(sizeof(int8_t) == 1, "");
static_assert(sizeof(int16_t) == 2, "");
static_assert(sizeof(int32_t) == 4, "");
static_assert(sizeof(int64_t) == 8, "");

using uint_least32_t = uint32_t;

static_assert(sizeof(uint_least32_t) >= 4, "");

using uintptr_t = uint32_t;

static_assert(sizeof(uintptr_t) >= sizeof(char *), "");

#define UINT8_C(x) (static_cast<uint8_t>(x))
#define UINT16_C(x) (static_cast<uint16_t>(x))
#define UINT32_C(x) (static_cast<uint32_t>(x))

#define UINT8_MAX (255)
#define UINT16_MAX (65535)
#define UINT32_MAX (4294967295U)
#define UINT64_MAX (18446744073709551615ULL)

#define INT32_MIN (-2147483648)

// Number of bits in a byte.
#define CHAR_BIT 8

#endif
