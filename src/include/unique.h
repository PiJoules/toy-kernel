#ifndef UNIQUE_H_
#define UNIQUE_H_

/**
 * Naive unique_ptr implementation (shortened to just `Unique`).
 */

#include <kalgorithm.h>
#include <ktype_traits.h>
#include <kutility.h>

namespace toy {

template <typename T>
class Unique {
 public:
  Unique() : data_(nullptr) {}
  Unique(T *data) : data_(data) {}
  Unique(Unique<T> &&other) : data_(nullptr) {
    Unique<T> tmp(other.release());
    tmp.swap(*this);
  }
  Unique<T> &operator=(Unique<T> &&other) {
    if (this != &other) {
      Unique<T> tmp(other.release());
      tmp.swap(*this);
    }
    return *this;
  }

  Unique(Unique<T> &other) = delete;
  Unique<T> &operator=(Unique<T> &other) = delete;

  void swap(Unique<T> &other) { toy::swap(data_, other.data_); }

  T *release() {
    T *released = nullptr;
    toy::swap(released, data_);
    return released;
  }

  ~Unique() {
    if (data_) delete data_;
  }

  T &operator*() const { return *data_; }
  T *operator->() const { return data_; }
  T *get() const { return data_; }

  operator bool() const { return data_; }

 private:
  T *data_;
};

template <typename T, typename... Args>
Unique<T> MakeUnique(Args &&...args) {
  return Unique<T>(new T(forward<Args>(args)...));
}

}  // namespace toy

#endif
