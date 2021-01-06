#ifndef KERNEL_H_
#define KERNEL_H_

/**
 * This header contains miscellanious things that can be used anywhere in the
 * kernel.
 */

#include <kstdint.h>
#include <print.h>
#include <serial.h>

#define __STR(s) STR(s)
#define STR(s) #s

inline void DisableInterrupts() { asm volatile("cli"); }
inline void EnableInterrupts() { asm volatile("sti"); }
inline bool InterruptsAreEnabled() {
  uint32_t eflags;
  asm volatile(
      "\
    pushf;\
    pop %0"
      : "=r"(eflags));
  return eflags & 0x200;
}

/**
 * RAII for disabling interrupts in a scope, then re-enabling them after exiting
 * the scope only if they were already enabled at the start.
 */
class DisableInterruptsRAII {
 public:
  DisableInterruptsRAII() : interrupts_enabled_(InterruptsAreEnabled()) {
    DisableInterrupts();
  }
  ~DisableInterruptsRAII() {
    if (interrupts_enabled_) EnableInterrupts();
  }

 private:
  bool interrupts_enabled_;
};

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

/**
 * Printing to serial.
 */
template <typename... Rest>
void DebugPrint(const char *str, Rest... rest) {
  print::Print(serial::Put, str, rest...);
}

#endif
