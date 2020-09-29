#ifndef KALGORITHM_H_
#define KALGORITHM_H_

#include <kutility.h>

namespace toy {

template <typename T>
constexpr const T &min(const T &a, const T &b) {
  return a < b ? a : b;
}

template <typename T>
constexpr const T &max(const T &a, const T &b) {
  return a > b ? a : b;
}

template <typename T>
void swap(T &a, T &b) {
  T temp = move(a);
  a = move(b);
  b = move(temp);
}

}  // namespace toy

#endif
