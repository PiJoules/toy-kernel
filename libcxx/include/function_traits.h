#ifndef FUNCTION_TRAITS_H_
#define FUNCTION_TRAITS_H_

/**
 * Function type traits implementation from
 * https://functionalcpp.wordpress.com/2013/08/05/function-traits/
 */

#include <tuple>

namespace std {

namespace ext {

template <typename F>
struct FunctionTraits;

// Function pointer
template <class R, class... Args>
struct FunctionTraits<R (*)(Args...)> : public FunctionTraits<R(Args...)> {};

template <class R, class... Args>
struct FunctionTraits<R(Args...)> {
  using return_type = R;
  static constexpr size_t num_args = sizeof...(Args);
  template <size_t N>
  struct argument {
    static_assert(N < num_args, "error: invalid parameter index.");
    using type = typename tuple_element<N, tuple<Args...>>::type;
  };
};

// member function pointer
template <class C, class R, class... Args>
struct FunctionTraits<R (C::*)(Args...)>
    : public FunctionTraits<R(C&, Args...)> {};

// const member function pointer
template <class C, class R, class... Args>
struct FunctionTraits<R (C::*)(Args...) const>
    : public FunctionTraits<R(C&, Args...)> {};

// member object pointer
template <class C, class R>
struct FunctionTraits<R(C::*)> : public FunctionTraits<R(C&)> {};

}  // namespace ext

}  // namespace std

#endif
