#ifndef RTTI_H_
#define RTTI_H_

#include <cassert>

/**
 * Inspired from LLVM's custom RTTI implementation.
 */

namespace rtti {

template <typename To, typename From>
bool isa(From *from) {
  return To::classof(from);
}

template <typename To, typename From>
To *cast(From *from) {
  assert(To::classof(from) && "Casting to invalid type.");
  // We can use a C-style cast here because it will select the appropriate cast
  // to use. In some cases, static_cast may be more important than
  // reinterpret_cast.
  return (To *)from;
}

template <typename To, typename From>
To *dyn_cast(From *from) {
  assert(
      from &&
      "Found a nullptr. Use dyn_cast_or_null if this value can be a nullptr.");
  if (!isa<To>(from)) return nullptr;
  return cast<To>(from);
}

template <typename To, typename From>
To *dyn_cast_or_null(From *from) {
  if (!from || !isa<To>(from)) return nullptr;
  return cast<To>(from);
}

// This assumes the class is using the enum-kind layout.
#define DEFINE_CLASSOF(Base, This)                                           \
  static bool classof(Base *base) {                                          \
    return This##_First <= base->getKind() && base->getKind() < This##_Last; \
  }
#define DEFINE_CLASSOF2(Base, This1, This2)     \
  static bool classof(Base *base) {             \
    return (This1##_First <= base->getKind() && \
            base->getKind() < This1##_Last) ||  \
           (This2##_First <= base->getKind() && \
            base->getKind() < This2##_Last);    \
  }

#define DEF_KIND_FIRST(This) This##_First, This##_RTTI_Kind = This##_First,

#define DEF_KIND_LAST(This) This##_Last,

#define DEF_KIND_LEAF(This) \
  DEF_KIND_FIRST(This)      \
  DEF_KIND_LAST(This)

#define RTTI_KIND(This) This##_RTTI_Kind

}  // namespace rtti

#endif
