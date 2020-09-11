/**
 * These are builtin functions taken from LLVM's compiler-rt. These are
 * specifically needed for clang to allow 64 bit operations on a 32 bit target.
 */
#include <kstdint.h>

#ifdef __clang__

#define clz(a) __builtin_clzll(a)

extern "C" {

uint64_t __udivdi3(uint64_t n, uint64_t d) {
  const unsigned N = sizeof(uint64_t) * CHAR_BIT;
  // d == 0 cases are unspecified.
  unsigned sr = (d ? clz(d) : N) - (n ? clz(n) : N);
  // 0 <= sr <= N - 1 or sr is very large.
  if (sr > N - 1)  // n < d
    return 0;
  if (sr == N - 1)  // d == 1
    return n;
  ++sr;
  // 1 <= sr <= N - 1. Shifts do not trigger UB.
  uint64_t r = n >> sr;
  n <<= N - sr;
  uint64_t carry = 0;
  for (; sr > 0; --sr) {
    r = (r << 1) | (n >> (N - 1));
    n = (n << 1) | carry;
    // Branch-less version of:
    // carry = 0;
    // if (r >= d) r -= d, carry = 1;
    const int64_t s = (int64_t)(d - r - 1) >> (N - 1);
    carry = s & 1;
    r -= d & s;
  }
  n = (n << 1) | carry;
  return n;
}

uint64_t __umoddi3(uint64_t n, uint64_t d) {
  const unsigned N = sizeof(uint64_t) * CHAR_BIT;
  // d == 0 cases are unspecified.
  unsigned sr = (d ? clz(d) : N) - (n ? clz(n) : N);
  // 0 <= sr <= N - 1 or sr is very large.
  if (sr > N - 1)  // n < d
    return n;
  if (sr == N - 1)  // d == 1
    return 0;
  ++sr;
  // 1 <= sr <= N - 1. Shifts do not trigger UB.
  uint64_t r = n >> sr;
  n <<= N - sr;
  uint64_t carry = 0;
  for (; sr > 0; --sr) {
    r = (r << 1) | (n >> (N - 1));
    n = (n << 1) | carry;
    // Branch-less version of:
    // carry = 0;
    // if (r >= d) r -= d, carry = 1;
    const int64_t s = (int64_t)(d - r - 1) >> (N - 1);
    carry = s & 1;
    r -= d & s;
  }
  return r;
}

}  // extern "C"

#else
#warning "Various builtin functions defined in " __FILE__ "may need to be implemented for this compiler"
#endif
