#ifndef ITERABLE_H_
#define ITERABLE_H_

#include <function_traits.h>
#include <initializer_list.h>
#include <kutility.h>

namespace toy {

template <typename ContainerTy>
class Iterable {
 public:
  using IteratorTy =
      typename FunctionTraits<decltype(&ContainerTy::begin)>::return_type;

  Iterable(ContainerTy &container) : container_(container) {}

  auto begin() const { return container_.begin(); }
  auto end() const { return container_.end(); }

 private:
  ContainerTy &container_;
};

template <typename ContainerTy>
class Enumerate {
 public:
  class EnumerateIterator {
   public:
    using IteratorTy = typename Iterable<ContainerTy>::IteratorTy;

    EnumerateIterator(IteratorTy iter) : first(0), second(iter) {}

    // Pre-increment
    EnumerateIterator &operator++() {
      ++first;
      ++second;
      return *this;
    }

    auto operator*() const {
      return Pair<size_t, typename IteratorTy::type>(first, *second);
    }

    bool operator==(const EnumerateIterator &other) const {
      return second == other.second;
    }
    bool operator!=(const EnumerateIterator &other) const {
      return second != other.second;
    }

   private:
    size_t first;
    IteratorTy second;
  };
  Enumerate(ContainerTy &container) : iterable_(container) {}

  EnumerateIterator begin() const {
    return EnumerateIterator(iterable_.begin());
  }
  EnumerateIterator end() const { return EnumerateIterator(iterable_.end()); }

 private:
  Iterable<ContainerTy> iterable_;
};

}  // namespace toy

#endif
