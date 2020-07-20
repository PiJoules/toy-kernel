#ifndef KTYPE_TRAITS_H_
#define KTYPE_TRAITS_H_

#include <kstdint.h>

namespace toy {

template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
  typedef T type;
};

template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <typename T>
struct remove_const {
  typedef T type;
};

template <typename T>
struct remove_const<const T> {
  typedef T type;
};

template <typename T>
struct remove_volatile {
  typedef T type;
};

template <typename T>
struct remove_volatile<volatile T> {
  typedef T type;
};

template <typename T>
struct remove_cv : remove_const<typename remove_volatile<T>::type> {};

template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

template <typename T>
struct is_unqualified_pointer {
  enum { value = false };
};

template <typename T>
struct is_unqualified_pointer<T *> {
  enum { value = true };
};

template <typename T>
struct is_pointer : is_unqualified_pointer<typename remove_cv<T>::type> {};

template <class T, T v>
struct integral_constant {
  static constexpr T value = v;
  using value_type = T;
  using type = integral_constant;  // using injected-class-name
  constexpr operator value_type() const noexcept { return value; }
  constexpr value_type operator()() const noexcept {
    return value;
  }  // since c++14
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template <class T, class U>
struct is_same : false_type {};

template <class T>
struct is_same<T, T> : true_type {};

template <class T>
struct remove_reference {
  typedef T type;
};
template <class T>
struct remove_reference<T &> {
  typedef T type;
};
template <class T>
struct remove_reference<T &&> {
  typedef T type;
};

template <bool B, class T, class F>
struct conditional {
  typedef T type;
};

template <bool B, class T, class F>
using conditional_t = typename conditional<B, T, F>::type;

template <class T, class F>
struct conditional<false, T, F> {
  typedef F type;
};

template <class T>
struct is_array : false_type {};

template <class T>
struct is_array<T[]> : true_type {};

template <class T, size_t N>
struct is_array<T[N]> : true_type {};

template <class T>
struct remove_extent {
  typedef T type;
};

template <class T>
struct remove_extent<T[]> {
  typedef T type;
};

template <class T, size_t N>
struct remove_extent<T[N]> {
  typedef T type;
};

template <class T>
struct is_const : false_type {};
template <class T>
struct is_const<const T> : true_type {};

template <class T>
struct is_reference : false_type {};
template <class T>
struct is_reference<T &> : true_type {};
template <class T>
struct is_reference<T &&> : true_type {};

template <class T>
struct is_function : integral_constant<bool, !is_const<const T>::value &&
                                                 !is_reference<T>::value> {};

template <typename>
struct is_integral_base : false_type {};
template <>
struct is_integral_base<bool> : true_type {};
template <>
struct is_integral_base<char> : true_type {};
template <>
struct is_integral_base<signed char> : true_type {};
template <>
struct is_integral_base<unsigned char> : true_type {};
template <>
struct is_integral_base<wchar_t> : true_type {};
template <>
struct is_integral_base<short> : true_type {};
template <>
struct is_integral_base<unsigned short> : true_type {};
template <>
struct is_integral_base<int> : true_type {};
template <>
struct is_integral_base<unsigned int> : true_type {};
template <>
struct is_integral_base<long> : true_type {};
template <>
struct is_integral_base<unsigned long> : true_type {};
template <>
struct is_integral_base<long long> : true_type {};
template <>
struct is_integral_base<unsigned long long> : true_type {};
#ifdef __SIZEOF_INT128__
template <>
struct is_integral_base<__int128_t> : true_type {};
template <>
struct is_integral_base<__uint128_t> : true_type {};
#endif
template <typename T>
struct is_integral : is_integral_base<remove_cv_t<T>> {};

namespace detail {

template <class T>
struct type_identity {
  using type = T;
};  // or use type_identity (since C++20)

template <class T>
auto try_add_pointer(int)
    -> type_identity<typename remove_reference<T>::type *>;
template <class T>
auto try_add_pointer(...) -> type_identity<T>;

}  // namespace detail

template <class T>
struct add_pointer : decltype(detail::try_add_pointer<T>(0)) {};

template <class T>
struct decay {
 private:
  typedef typename remove_reference<T>::type U;

 public:
  typedef typename conditional<
      is_array<U>::value, typename remove_extent<U>::type *,
      typename conditional<is_function<U>::value, typename add_pointer<U>::type,
                           typename remove_cv<U>::type>::type>::type type;
};

template <class...>
struct disjunction : false_type {};
template <class B1>
struct disjunction<B1> : B1 {};
template <class B1, class... Bn>
struct disjunction<B1, Bn...>
    : conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};

template <class...>
struct conjunction : true_type {};
template <class B1>
struct conjunction<B1> : B1 {};
template <class B1, class... Bn>
struct conjunction<B1, Bn...>
    : conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template <class T>
struct is_void : is_same<void, typename remove_cv<T>::type> {};

template <typename T>
struct pointer_traits : false_type {};

template <typename T>
struct pointer_traits<T *> : true_type {
  using pointee_type = T;
};

template <class T>
struct is_void_ptr : false_type {};

template <class T>
struct is_void_ptr<T *>
    : enable_if<
          is_void<typename pointer_traits<T>::pointee_type>::value>::type {};

}  // namespace toy

#endif
