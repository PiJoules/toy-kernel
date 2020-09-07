#ifndef KERNEL_H_
#define KERNEL_H_

#include <kstdint.h>

#define __STR(s) STR(s)
#define STR(s) #s

inline void DisableInterrupts() { asm volatile("cli"); }
inline void EnableInterrupts() { asm volatile("sti"); }
inline bool InteruptsAreEnabled() {
  uint32_t eflags;
  asm volatile(
      "\
    pushf;\
    pop %0"
      : "=r"(eflags));
  return eflags & 0x200;
}

inline bool IsPowerOf2(uint32_t x) { return x != 0 && ((x & (x - 1)) == 0); }

template <typename T>
constexpr T ipow(T num, uint32_t power) {
  return power ? ipow(num, power - 1) * num : 1;
}

template <typename T>
constexpr T ipow2(uint32_t power) {
  return ipow(T(2), power);
}

// We must clear interrupt flags, otherwise we could hit an interrupt that
// triggers on the infinite loop if interrupts were previously enabled.
#define LOOP_INDEFINITELY() \
  DisableInterrupts();      \
  while (1) {}

#ifdef __clang__
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE()
#endif

#endif
