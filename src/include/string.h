#ifndef STRING_H_
#define STRING_H_

/**
 * Naive string implementation.
 */

#include <kalgorithm.h>
#include <kstring.h>
#include <vector.h>

namespace toy {

class String {
 public:
  String()
      : size_(0),
        capacity_(kDefaultCapacity),
        data_(toy::kcalloc<char>(capacity_)) {
    memset(data_, 0, capacity_);
  }
  String(const char *s, size_t init_capacity = kDefaultCapacity)
      : size_(strlen(s)),
        capacity_(max(init_capacity, size_ + 1)),
        data_(toy::kcalloc<char>(capacity_)) {
    memcpy(data_, s, size_);
  }

  ~String() { kfree(data_); }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  void push_back(char c) {
    ++size_;
    if (size_ > capacity_) {
      // Resize.
      capacity_ <<= 1;
      assert(capacity_ >= size_ && "Not enough capacity after push_back");
      data_ = toy::krealloc<char>(data_, capacity_);
      assert(data_ && "Could not request more storage");
      memset(&data_[size_], 0, capacity_ - size_);
    }
    data_[size_ - 1] = c;
  }

  char &operator[](size_t index) const { return data_[index]; }

  const char *c_str() const { return data_; }

  bool operator==(const char *other) const { return strcmp(data_, other) == 0; }
  bool operator!=(const char *other) const { return !(operator==(other)); }

 private:
  size_t size_;
  size_t capacity_;
  char *data_;
};

}  // namespace toy

#endif
