#ifndef KTUPLE_H_
#define KTUPLE_H_

namespace toy {

template <typename First, typename... Rest>
struct Tuple : public Tuple<Rest...> {
  Tuple(First first, Rest... rest) : Tuple<Rest...>(rest...), first(first) {}

  First first;
};

template <typename First>
struct Tuple<First> {
  Tuple(First first) : first(first) {}

  First first;
};

template <size_t I, class T>
struct TupleElement;

// recursive case
template <size_t I, class Head, class... Tail>
struct TupleElement<I, Tuple<Head, Tail...>>
    : TupleElement<I - 1, Tuple<Tail...>> {};

// base case
template <class Head, class... Tail>
struct TupleElement<0, Tuple<Head, Tail...>> {
  typedef Head type;
};

}  // namespace toy

#endif
