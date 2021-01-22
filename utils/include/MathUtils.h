#ifndef MATH_UTILS_H_
#define MATH_UTILS_H_

#include <stdint.h>

namespace utils {

inline bool IsPowerOf2(uint32_t x) { return x != 0 && ((x & (x - 1)) == 0); }

template <typename T>
constexpr T ipow(T num, uint32_t power) {
  return power ? ipow(num, power - 1) * num : 1;
}

template <typename T>
constexpr T ipow2(uint32_t power) {
  return ipow(T(2), power);
}

// If the value is not a power of 2, return the smallest power of 2 greater than
// this value. Otherwise, return the value itself.
template <typename T>
T NextPowOf2(T x) {
  if (!x) return 0;

  if (IsPowerOf2(x)) return x;

  size_t shifts = 0;
  while (x != 1) {
    ++shifts;
    x >>= 1;
  }
  return T(x) << (shifts + 1);
}

}  // namespace utils

#endif
