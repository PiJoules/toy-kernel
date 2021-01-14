#ifndef ITERATOR_H_
#define ITERATOR_H_

/**
 * Common iterators that can be used by containers.
 */

namespace std {

namespace ext {

template <typename T>
class PointerIterator {
 public:
  using type = T;

  PointerIterator(T *ptr) : ptr_(ptr) {}

  // Pre-increment
  PointerIterator &operator++() {
    ++ptr_;
    return *this;
  }

  T &operator*() const { return *ptr_; }

  T *operator->() const { return ptr_; }

  bool operator==(const PointerIterator &other) const {
    return ptr_ == other.ptr_;
  }
  bool operator!=(const PointerIterator &other) const {
    return ptr_ != other.ptr_;
  }

 private:
  T *ptr_;
};

}  // namespace ext

}  // namespace std

#endif
