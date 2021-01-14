#ifndef UTILITY_H_
#define UTILITY_H_

#include <type_traits.h>

namespace std {

template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&arg) noexcept {
  return static_cast<typename remove_reference<T>::type &&>(arg);
}

template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &arg) noexcept {
  return static_cast<T &&>(arg);
}

template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &&arg) noexcept {
  static_assert(!is_lvalue_reference<T>::value,
                "invalid rvalue to lvalue conversion");
  return static_cast<T &&>(arg);
}

template <typename T1, typename T2>
struct pair {
  T1 first;
  T2 second;

  pair(const T1 &first, const T2 &second) : first(first), second(second) {}
};

}  // namespace std

#endif
