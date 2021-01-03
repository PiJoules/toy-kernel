#ifndef PRINT_H_
#define PRINT_H_

#include <kassert.h>
#include <ktype_traits.h>

/**
 * This header contains utilities for print formatting.
 */

namespace print {

/**
 * This type represents a callback that is ultimately used when printing a
 * character. This callback exists for the purpose of handling newlines and
 * scrolling when appropriate.
 */
using PutFunc = void (*)(char c);

/**
 * Call `put` on each character in `str`.
 */
void Print(PutFunc put, const char* str);

/**
 * PrintFormatter is a helper class used to specify how a type should be
 * printed. For a custom type, T, just define a new PrintForatter:
 *
 *   template <>
 *   void PrintFormatter(PutFunc put, T val) {...}
 */
template <typename T1, toy::enable_if_t<!toy::is_pointer<T1>::value, int> = 0>
void PrintFormatter(PutFunc put, T1 val);

/**
 * Hex is a wrapper class for integral values that would like to be printed in
 * hexadecimal notation.
 *
 *   using terminal::Hex;
 *   terminal::WriteF("Hex value: {}\n", Hex(1));
 */
template <typename T, toy::enable_if_t<toy::is_integral<T>::value, int> = 0>
struct Hex {
  Hex(T val) : val(val) {}
  T val;
};

template <typename T>
struct is_string
    : public toy::disjunction<
          toy::is_same<char*, typename toy::decay<T>::type>,
          toy::is_same<const char*, typename toy::decay<T>::type> > {};

template <typename T1,
          toy::enable_if_t<toy::is_pointer<T1>::value && !is_string<T1>::value,
                           int> = 0>
void PrintFormatter(PutFunc put, T1 val) {
  PrintFormatter(put, Hex(reinterpret_cast<uintptr_t>(val)));
}

template <typename T1, toy::enable_if_t<is_string<T1>::value, int> = 0>
void PrintFormatter(PutFunc put, T1 val);

template <typename T1>
void Print(PutFunc put, const char* data, T1 val) {
  char c;
  while ((c = *data) != '\0') {
    switch (c) {
      case '{':
        PrintFormatter(put, val);
        ++data;
        if (*data != '}') __builtin_trap();
        break;
      default:
        put(c);
    }
    ++data;
  }
}

template <typename T1, typename... Rest>
void Print(PutFunc put, const char* data, T1 val, Rest... rest) {
  char c;
  while ((c = *data) != '\0') {
    switch (c) {
      case '{':
        PrintFormatter(put, val);
        ++data;
        assert(*data == '}' && "Missing closing '}'");
        return Print(put, ++data, rest...);
      default:
        put(c);
    }
    ++data;
  }
}

}  // namespace print

#endif
