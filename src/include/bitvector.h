#ifndef BITVECTOR_H_
#define BITVECTOR_H_

#include <kmalloc.h>

namespace toy {

class BitVector {
 public:
  BitVector(size_t bits) : bits_(bits), capacity_(getCapacity(bits)) { Init(); }
  BitVector() : BitVector(0) {}

  ~BitVector() {
    if (onHeap()) kfree(reinterpret_cast<void *>(buffer_));
  }

  void push_back(bool x) {
    IncrementSize();
    set(bits_ - 1, x);
  }

  void pop_back() {
    assert(!empty());
    --bits_;
  }

  bool get(size_t bit) const {
    assert(bit < bits_);
    if (onHeap()) {
      auto bits_idx = bit / CHAR_BIT;
      auto shift_amt = bit % CHAR_BIT;
      uint8_t &byte = BufferPtr()[bits_idx];
      return byte & (UINT8_C(1) << shift_amt);
    } else {
      return buffer_ & (uintptr_t(1) << bit);
    }
  }

  bool getBack() const { return get(bits_ - 1); }

  void set(size_t bit, bool val = true) {
    assert(bit < bits_);
    if (onHeap()) {
      auto bits_idx = bit / CHAR_BIT;
      auto shift_amt = bit % CHAR_BIT;
      uint8_t &byte = BufferPtr()[bits_idx];
      byte = static_cast<uint8_t>((byte & ~(UINT8_C(1) << shift_amt)) |
                                  (UINT8_C(val) << shift_amt));
    } else {
      buffer_ = (buffer_ & ~(uintptr_t(val) << bit)) | (uintptr_t(val) << bit);
    }
  }

  size_t size() const { return bits_; }
  bool empty() const { return !bits_; }

  template <typename T>
  T getAs() const {
    assert(sizeof(T) * CHAR_BIT >= bits_ &&
           "Cannot fit this value onto this type");
    if (onHeap()) {
      T x;
      uint8_t *data_ptr = reinterpret_cast<uint8_t *>(&x);
      memset(data_ptr, 0, sizeof(T));
      for (size_t bit = 0; bit < bits_; ++bit ) {
        auto bits_idx = bit / CHAR_BIT;
        auto shift_amt = bit % CHAR_BIT;
        uint8_t &byte = data_ptr[bits_idx];
        uint8_t val = BufferPtr()[bits_idx] & (UINT8_C(1) << shift_amt);
        byte = static_cast<uint8_t>((byte & ~(UINT8_C(1) << shift_amt)) | val);
      }
      return x;
    } else {
      return static_cast<T>(buffer_);
    }
  }

 private:
  size_t bits_;
  size_t capacity_;  // In bytes

  // If the capacity of this bitvector is greater than the size of the buffer,
  // then this instead represents a pointer to a malloc'd address.
  uintptr_t buffer_;

  uint8_t *BufferPtr() const {
    assert(onHeap());
    return reinterpret_cast<uint8_t *>(buffer_);
  }

  static constexpr size_t kDefaultSize = sizeof(buffer_) * CHAR_BIT;

  static size_t BytesNeeded(size_t bits) {
    if (bits % CHAR_BIT) return (bits / CHAR_BIT) + 1;
    return bits / CHAR_BIT;
  }

  static size_t getCapacity(size_t bits) {
    size_t bytes = BytesNeeded(bits);
    if (bytes <= sizeof(buffer_)) return sizeof(buffer_);
    return NextPowOf2(bytes) * 2;
  }

  void Init() {
    if (onHeap()) {
      buffer_ = reinterpret_cast<uintptr_t>(::kmalloc(capacity_));
    } else {
      buffer_ = 0;
    }
  }

  bool onHeap() const { return capacity_ > sizeof(buffer_); }

  void IncrementSize() {
    ++bits_;
    if (BytesNeeded(bits_) > capacity_) {
      bool was_on_heap = onHeap();
      capacity_ <<= 1;
      assert(capacity_ >= BytesNeeded(bits_));

      if (was_on_heap) {
        assert(onHeap() &&
               "Cannot have been on the heap but then come off the heap");
        buffer_ = reinterpret_cast<uintptr_t>(
            ::krealloc(reinterpret_cast<void *>(buffer_), capacity_));
      } else if (onHeap()) {
        auto val = buffer_;
        buffer_ = reinterpret_cast<uintptr_t>(::kmalloc(capacity_));
        memcpy(BufferPtr(), &val, sizeof(val));
      }
    }
  }
};

}  // namespace toy

#endif
