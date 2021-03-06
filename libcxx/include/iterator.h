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

  T *get() const { return ptr_; }

  bool operator==(const PointerIterator &other) const {
    return ptr_ == other.ptr_;
  }
  bool operator!=(const PointerIterator &other) const {
    return ptr_ != other.ptr_;
  }

  bool operator<(const PointerIterator<T> &other) const {
    return ptr_ < other.ptr_;
  }
  bool operator<=(const PointerIterator<T> &other) const {
    return ptr_ <= other.ptr_;
  }
  bool operator>(const PointerIterator<T> &other) const {
    return ptr_ > other.ptr_;
  }
  bool operator>=(const PointerIterator<T> &other) const {
    return ptr_ >= other.ptr_;
  }

  size_t operator-(const PointerIterator<T> &other) const {
    return ptr_ - other.ptr_;
  }
  PointerIterator<T> operator-(int64_t i) const { return ptr_ - i; }

  PointerIterator<T> operator+(int i) const { return ptr_ + i; }

 private:
  T *ptr_;
};

}  // namespace ext

}  // namespace std

#endif
