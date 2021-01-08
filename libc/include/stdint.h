#ifndef STDINT_H_
#define STDINT_H_

typedef __SIZE_TYPE__ size_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
static_assert(sizeof(uint8_t) == 1);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(uint32_t) == 4);

typedef int int32_t;
static_assert(sizeof(int32_t) == 4);

#define INT32_MIN (-2147483648)

#endif
