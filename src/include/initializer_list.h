#ifndef INITIALIZER_LIST_H_
#define INITIALIZER_LIST_H_

/**
 * Naive header-only implementation of std::initializer_list.
 *
 * Note that the compiler will only accept a list-initialization to
 * initializer_list-like object *iff* the initialize_list-like object is
 * explicitly of type "std::initializer_list".
 */

#include <iterator.h>
#include <kstdint.h>

namespace std {

template <typename T>
class initializer_list {
 public:
  size_t size() const { return size_; }
  toy::PointerIterator<T> begin() const {
    return toy::PointerIterator<T>(begin_);
  }
  toy::PointerIterator<T> end() const {
    return toy::PointerIterator<T>(begin_ + size_);
  }
  const T &operator[](size_t i) const { return begin_[i]; }

 private:
  initializer_list(const T *begin, size_t size) : begin_(begin), size_(size) {}

  const T *begin_;
  size_t size_;
};

}  // namespace std

namespace toy {

template <typename T>
using InitializerList = std::initializer_list<T>;

}  // namespace toy

#endif
