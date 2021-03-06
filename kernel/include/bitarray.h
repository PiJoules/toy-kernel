#ifndef BIT_ARRAY_H_
#define BIT_ARRAY_H_

// FIXME: Move this to utils.

#include <kernel.h>
#include <stdint.h>
#include <string.h>

namespace toy {

template <size_t NumBits>
class BitArray {
 public:
  size_t size() const { return NumBits; }

  void Clear() { memset(bitarray_, 0, sizeof(bitarray_)); }

  void setOne(size_t bit) {
    assert(bit < NumBits);
    bitarray_[bit / 8] |= (UINT32_C(1) << (bit % 8));
  }

  void setZero(size_t bit) {
    assert(bit < NumBits);
    bitarray_[bit / 8] &= ~(UINT32_C(1) << (bit % 8));
  }

  bool isSet(size_t bit) const {
    return bitarray_[bit / 8] & (UINT8_C(1) << (bit % 8));
  }

  /**
   * Set the bits after the `n`th bit to one.
   */
  void Reserve(size_t n) {
    assert(n <= NumBits);
    for (size_t i = n; i < NumBits; ++i) setOne(i);
  }

  /**
   * Get the first zero in the bit array.
   *
   * `start` specifies the first bit we can search from.
   *
   * If no zero is found (the entire array is filled with 1s), return false.
   * Otherwise return true on successfully finding a 0.
   */
  bool GetFirstZero(size_t &bit, size_t start = 0) const {
    for (bit = start; bit < NumBits; ++bit) {
      if (!isSet(bit)) return true;
    }
    return false;
  }

  const uint8_t *get() const { return bitarray_; }

  void Dump() const {
    // FIXME: We should be able to print up to 64-bit ints with this to save
    // some space. Also when we move this out of the kernel and into utils, this
    // method should go away and there should be kernel/userspace-specific
    // functions for dumping.
    for (size_t i = 0; i < NumBits / 8; ++i) {
      DebugPrint("{} ", print::Hex(bitarray_[i]));
    }
  }

 protected:
  uint8_t bitarray_[NumBits / 8];
};

}  // namespace toy

#endif
