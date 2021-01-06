#ifndef KUTILITY_H_
#define KUTILITY_H_

#include <ktype_traits.h>

namespace toy {

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
struct Pair {
  T1 first;
  T2 second;

  Pair(const T1 &first, const T2 &second) : first(first), second(second) {}
};

}  // namespace toy

#endif
