#ifndef VECTOR_H_
#define VECTOR_H_

#include <initializer_list.h>
#include <iterator.h>
#include <kalgorithm.h>
#include <kmalloc.h>
#include <knew.h>

/**
 * Naive header-only vector implementation.
 *
 * Note that alignment of data stored in by default will use kMaxAlignment.
 */

namespace toy {

constexpr size_t kDefaultCapacity = 8;

template <typename T, size_t InitCapacity = kDefaultCapacity>
class Vector {
 public:
  static_assert(InitCapacity > 0,
                "The initial vector capacity should be non-zero.");
  Vector()
      : size_(0), capacity_(InitCapacity), data_(toy::kmalloc<T>(capacity_)) {}

  /**
   * This is undefined if [start, end) is invalid.
   */
  Vector(const T *start, const T *end)
      : size_(end - start),
        capacity_(max(InitCapacity, size_)),
        data_(toy::kmalloc<T>(capacity_)) {
    assert(start <= end && "Invalid pointer range");
    for (size_t i = 0; i < size_; ++i) { new (&data_[i]) T(start[i]); }
  }

  Vector(toy::InitializerList<T> l)
      : size_(l.size()),
        capacity_(max(InitCapacity, size_)),
        data_(toy::kmalloc<T>(capacity_)) {
    for (size_t i = 0; i < size_; ++i) { new (&data_[i]) T(l[i]); }
  }

  Vector(const Vector<T> &other)
      : size_(other.size()),
        capacity_(max(InitCapacity, other.size_)),
        data_(toy::kmalloc<T>(capacity_)) {
    for (size_t i = 0; i < size_; ++i) { new (&data_[i]) T(other[i]); }
  }

  Vector(Vector<T> &&other)
      : size_(other.size_),
        capacity_(other.capacity_),
        data_(toy::kmalloc<T>(capacity_)) {
    for (size_t i = 0; i < size_; ++i) { new (&data_[i]) T(move(other[i])); }

    // After the move, `other` is guaranteed to be empty().
    other.size_ = 0;
  }

  ~Vector() {
    for (size_t i = 0; i < size_; ++i) { data_[i].~T(); }
    kfree(data_);
  }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  void push_back(const T &item) {
    IncrementSize();
    new (&data_[size_ - 1]) T(item);
  }
  void push_back(T &&item) {
    IncrementSize();
    new (&data_[size_ - 1]) T(move(item));
  }

  T &operator[](size_t index) { return data_[index]; }
  const T &operator[](size_t index) const { return data_[index]; }

  T &back() { return data_[size_ - 1]; }
  const T &back() const { return data_[size_ - 1]; }

  PointerIterator<T> begin() const { return PointerIterator(data_); }
  PointerIterator<T> end() const { return PointerIterator(data_ + size_); }

  const T *data() const { return data_; }

 private:
  void IncrementSize() {
    ++size_;
    if (size_ > capacity_) {
      // Resize.
      capacity_ <<= 1;
      assert(capacity_ >= size_ && "Not enough capacity after push_back");
      data_ = toy::krealloc<T>(data_, capacity_);
      assert(data_ && "Could not request more storage");
    }
  }

  size_t size_;
  size_t capacity_;
  T *data_;
};

}  // namespace toy

#endif
