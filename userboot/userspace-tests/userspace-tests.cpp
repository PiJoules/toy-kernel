#include <BitVector.h>
#include <MathUtils.h>
#include <_syscalls.h>
#include <allocator.h>
#include <iterable.h>
#include <print.h>
#include <rtti.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

extern bool __use_debug_log;

namespace print {

// FIXME: This is used by the VectorFind test. This should not be defined here.
template <>
void PrintFormatter(print::PutFunc put, std::ext::PointerIterator<int> it) {
  return PrintFormatter(put, &*it);
}
template <>
void PrintFormatter(print::PutFunc put, std::string s) {
  return PrintFormatter(put, s.c_str());
}
template <>
void PrintFormatter(print::PutFunc put, std::ext::PointerIterator<char> it) {
  return PrintFormatter(put, &*it);
}

}  // namespace print

namespace {

void PrintStdout(const char *data) { print::Print(put, data); }

template <typename T1>
void PrintStdout(const char *data, T1 val) {
  print::Print(put, data, val);
}

template <typename T1, typename... Rest>
void PrintStdout(const char *data, T1 val, Rest... rest) {
  print::Print(put, data, val, rest...);
}

// FIXME: This will lead to a crash if we remove this from an empty namespace.
#define PRINT PrintStdout
#include <tests.h>

using rtti::cast;
using rtti::dyn_cast;
using rtti::isa;
using utils::BitVector;
using utils::GetHeapUsed;
using utils::MallocHeader;
using vfs::Directory;
using vfs::File;
using vfs::Node;

TEST(IntegerPower) {
  for (uint32_t p = 0, expected = 1; p < 31; ++p, expected <<= 1) {
    ASSERT_EQ(utils::ipow2<uint32_t>(p), expected);
  }
}

TEST_SUITE(MathFunctions) { RUN_TEST(IntegerPower); }

TEST(Memset) {
  size_t size = 10;
  char buffer[size];
  buffer[9] = 9;
  char expected = 'a';
  memset(buffer, expected, size);
  for (unsigned i = 0; i < size; ++i) ASSERT_EQ(buffer[i], expected);
}

TEST(Memcpy) {
  size_t size = 10;
  char buffer[size], buffer2[size];
  memset(buffer, 1, size);
  memset(buffer2, 2, size);
  memcpy(buffer2, buffer, size);
  for (unsigned i = 0; i < size; ++i) ASSERT_EQ(buffer2[i], 1);
}

TEST(Strlen) {
  char str[] = "buffer\0\0";
  ASSERT_EQ(strlen(str), 6u);
}

TEST(Strcmp) {
  size_t size = 10;
  char str1[size], str2[size];
  str1[size - 1] = 0;
  str2[size - 1] = 0;
  memset(str1, 'a', size - 1);
  memset(str2, 'b', size - 1);

  str2[0] = 'a';
  ASSERT_EQ(strcmp(str1, str2), -1);

  str1[0] = 'c';
  ASSERT_EQ(strcmp(str1, str2), 2);

  memset(str1, 'b', size - 1);
  memset(str2, 'b', size - 1);
  ASSERT_EQ(strcmp(str1, str2), 0);
}

// FIXME: These should be packaged with libc.
TEST_SUITE(CString) {
  RUN_TEST(Memset);
  RUN_TEST(Memcpy);
  RUN_TEST(Strlen);
  RUN_TEST(Strcmp);
}

char Buffer[1024];
char *BufferPtr = Buffer;

void PutBuffer(char c) {
  *BufferPtr = c;
  ++BufferPtr;
}

void PrintingSetup() {
  BufferPtr = Buffer;
  memset(Buffer, 0, sizeof(Buffer) * sizeof(char));
}

TEST(StringFormatting) {
  print::Print(PutBuffer, "hello {}", "world");
  ASSERT_STREQ(Buffer, "hello world");
}

TEST(IntFormatting) {
  print::Print(PutBuffer, "12{}4", 3);
  ASSERT_STREQ(Buffer, "1234");
}

TEST(UnsignedFormatting) {
  print::Print(PutBuffer, "12{}4", 3u);
  ASSERT_STREQ(Buffer, "1234");
}

// FIXME: These should be packaged with utils.
TEST_SUITE(Printing) {
  SETUP(PrintingSetup);
  RUN_TEST(StringFormatting);
  RUN_TEST(IntFormatting);
  RUN_TEST(UnsignedFormatting);
}

TEST(MinAllocation) {
  size_t heap_used = GetHeapUsed();
  size_t size = sizeof(char) * 4;
  auto *str = static_cast<char *>(malloc(size));

  // Check allocation size.
  ASSERT_EQ(GetHeapUsed(), heap_used + size + sizeof(MallocHeader));

  // Make sure we can read and write expectedly.
  str[0] = 'a';
  str[1] = 'b';
  str[2] = 'c';
  str[3] = '\0';
  ASSERT_STREQ(str, "abc");

  // Should be freed.
  free(str);
  ASSERT_EQ(GetHeapUsed(), heap_used);
}

TEST(MoreThanMinAllocation) {
  size_t heap_used = GetHeapUsed();
  size_t size = utils::kMallocMinSize * 2;
  auto *str = static_cast<char *>(malloc(sizeof(char) * size));

  // We get the amount we request + the malloc header.
  ASSERT_EQ(GetHeapUsed(), heap_used + size + sizeof(MallocHeader));

  // Make sure we can write to all of it.
  memset(str, 'a', size);

  free(str);
  ASSERT_EQ(GetHeapUsed(), heap_used);
}

TEST(MultipleAllocations) {
  size_t size = utils::kMallocMinSize;
  size_t heap_used = GetHeapUsed();
  void *buf1 = malloc(size);
  ASSERT_EQ(GetHeapUsed(), heap_used + (size + sizeof(MallocHeader)));
  void *buf2 = malloc(size);
  ASSERT_EQ(GetHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 2);
  void *buf3 = malloc(size);
  ASSERT_EQ(GetHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 3);

  memset(buf1, 'a', size);
  memset(buf2, 'a', size);
  memset(buf3, 'a', size);

  free(buf1);
  ASSERT_EQ(GetHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 2);
  free(buf2);
  ASSERT_EQ(GetHeapUsed(), heap_used + (size + sizeof(MallocHeader)));
  free(buf3);
  ASSERT_EQ(GetHeapUsed(), heap_used);
}

// This is a regression test to make sure I don't run into a bug in malloc that
// I ran into one when allocating tasks.
TEST(TaskAllocation) {
  size_t size = 32;

  auto heap_used = GetHeapUsed();
  auto *alloc1 = (uint8_t *)malloc(size);

  // Size of allocation + size of malloc header
  ASSERT_EQ(MallocHeader::FromPointer(alloc1)->size,
            size + sizeof(MallocHeader));
  ASSERT_EQ(GetHeapUsed(), heap_used + size + sizeof(MallocHeader));

  uint8_t size2 = 1;
  auto *alloc2 = (uint8_t *)malloc(size2);

  // The allocations should have a size at least greater than what was
  // requested.
  ASSERT_GE(MallocHeader::FromPointer(alloc2)->size,
            size2 + sizeof(MallocHeader));
  ASSERT_EQ(GetHeapUsed(), heap_used + MallocHeader::FromPointer(alloc1)->size +
                               MallocHeader::FromPointer(alloc2)->size);

  free(alloc1);
  ASSERT_EQ(GetHeapUsed(), heap_used + MallocHeader::FromPointer(alloc2)->size);
  free(alloc2);
  ASSERT_EQ(GetHeapUsed(), heap_used);
}

TEST(Alignment) {
  void *x = malloc(1000);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(x) % kMaxAlignment, 0);
  free(x);

  uint32_t alignment = 4096;
  x = malloc(alignment * 2, alignment);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(x) % alignment, 0);
  free(x);
}

TEST(Realloc) {
  size_t heap_used = GetHeapUsed();
  size_t init_size = 10;
  void *ptr = malloc(init_size);
  ASSERT_GE(GetHeapUsed(), heap_used + 10 + sizeof(MallocHeader));

  // Re-usable pointer for size 0 realloc.
  ASSERT_EQ(realloc(ptr, 0), nullptr);
  auto *chunk = MallocHeader::FromPointer(ptr);
  ASSERT_EQ(chunk->used, 1);
  ASSERT_GE(chunk->size, init_size + sizeof(MallocHeader));

  // Same size allocation.
  ASSERT_EQ(realloc(ptr, init_size), ptr);

  // Increased size. It may or may not be the same pointer though.
  size_t newsize = 1024;
  void *newptr = realloc(ptr, newsize);
  ASSERT_NE(newptr, ptr);
  chunk = MallocHeader::FromPointer(ptr);
  auto *chunk2 = MallocHeader::FromPointer(newptr);
  ASSERT_EQ(chunk->used, 0);
  ASSERT_EQ(chunk2->used, 1);
  ASSERT_EQ(chunk2->size, newsize + sizeof(MallocHeader));

  // Decrease size. This is a large enough decrease that the pointer may not
  // change.
  ptr = realloc(newptr, init_size);
  ASSERT_EQ(ptr, newptr);
  ASSERT_EQ(chunk2->used, 1);
  ASSERT_GE(chunk2->size, init_size + sizeof(MallocHeader));

  free(newptr);
  ASSERT_EQ(heap_used, GetHeapUsed());
}

TEST(ReallocDataCopied) {
  size_t s = 10;
  size_t s2 = 1000;
  char *ptr = (char *)malloc(s);
  memset(ptr, 'a', s);
  char *ptr2 = (char *)realloc(ptr, s2);
  ASSERT_NE(ptr2, nullptr);
  ASSERT_NE(ptr, ptr2);
  for (int i = 0; i < s; ++i) ASSERT_EQ(ptr2[i], 'a');
  free(ptr2);
}

TEST_SUITE(Malloc) {
  RUN_TEST(MinAllocation);
  RUN_TEST(MoreThanMinAllocation);
  RUN_TEST(MultipleAllocations);
  RUN_TEST(TaskAllocation);
  RUN_TEST(Alignment);
  RUN_TEST(Realloc);
  RUN_TEST(ReallocDataCopied);
}

TEST(CallocTest) {
  auto *ptr = reinterpret_cast<int *>(calloc(4, sizeof(int)));
  for (int i = 0; i < 4; ++i) ASSERT_EQ(ptr[i], 0);
  free(ptr);
  ptr = reinterpret_cast<int *>(calloc(4, sizeof(int)));
  for (int i = 0; i < 4; ++i) ASSERT_EQ(ptr[i], 0);
  free(ptr);
}

TEST_SUITE(Calloc) { RUN_TEST(CallocTest); }

TEST(VirtualMethods) {
  struct A {
    virtual int get() const { return 1; }
  };
  struct B : public A {
    int get() const override { return 2; }
  };
  struct C : public A {
    int get() const override { return 3; }
  };
  A a;
  B b;
  C c;
  A *aptr = &a;
  ASSERT_EQ(aptr->get(), 1);

  aptr = &b;
  ASSERT_EQ(aptr->get(), 2);

  aptr = &c;
  ASSERT_EQ(aptr->get(), 3);

  struct D {
    // __cxa_pure_virtual will need to be defined.
    virtual int get() const = 0;
  };
  struct E : public D {
    int get() const override { return 1; }
  };
  E e;
  D *dptr = &e;
  ASSERT_EQ(dptr->get(), 1);
}

TEST(New) {
  struct A {
    int32_t x, y;
  };

  size_t heap_used = GetHeapUsed();
  A *a = new A;
  ASSERT_EQ(GetHeapUsed(), heap_used + sizeof(A) + sizeof(MallocHeader));
  delete a;
  ASSERT_EQ(GetHeapUsed(), heap_used);

  struct B {
    int32_t x;
    alignas(1024) int32_t arr[2];
  };
  B *b = new B;  // void *operator new(size_t size, std::align_val_t alignment)
  ASSERT_GE(GetHeapUsed(), heap_used + sizeof(B) + sizeof(MallocHeader));
  delete b;
  ASSERT_EQ(GetHeapUsed(), heap_used);

  struct C {
    uint8_t x, y;
  };
  void *buffer = malloc(sizeof(C));
  ASSERT_GE(GetHeapUsed(), heap_used + sizeof(C) + sizeof(MallocHeader));

  C *c = new (buffer) C;
  ASSERT_GE(GetHeapUsed(), heap_used + sizeof(C) + sizeof(MallocHeader));

  c->x = 1;
  c->y = 2;
  C *casted = static_cast<C *>(buffer);
  ASSERT_EQ(casted->x, 1);
  ASSERT_EQ(casted->y, 2);

  // Freed after a normal delete.
  delete c;
  ASSERT_EQ(GetHeapUsed(), heap_used);
}

TEST_SUITE(CppLangFeatures) {
  RUN_TEST(VirtualMethods);
  RUN_TEST(New);
}

TEST(PushBack) {
  std::vector<int> v;
  ASSERT_TRUE(v.empty());
  v.push_back(1);
  ASSERT_EQ(v.size(), 1);
  ASSERT_EQ(v[0], 1);
  v.push_back(2);
  v.push_back(3);
  ASSERT_EQ(v.size(), 3);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);

  std::vector<int, /*InitCapacity=*/1> v2;
  v2.push_back(1);
  v2.push_back(10);  // Triggers reallocation.
  ASSERT_EQ(v2.size(), 2);
  ASSERT_EQ(v2[0], 1);
  ASSERT_EQ(v2[1], 10);
}

struct S {
  static int copy_ctor_calls;
  S() {}
  S(const S &) { ++copy_ctor_calls; }
};
int S::copy_ctor_calls = 0;

TEST(VectorRangeCtor) {
  // Under capacity
  int x[] = {1, 2, 3};
  std::vector<int, /*InitCapacity=*/8> v(x, x + 3);
  ASSERT_EQ(v.size(), 3);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);

  // More than capacity
  std::vector<int, 1> v2(x, x + 3);
  ASSERT_EQ(v2.size(), 3);
  ASSERT_EQ(v2[0], 1);
  ASSERT_EQ(v2[1], 2);
  ASSERT_EQ(v2[2], 3);

  // Ensure constructors are called
  S s[] = {S(), S(), S()};
  std::vector<S> v3(s, s + 3);
  ASSERT_EQ(v2.size(), 3);
  ASSERT_EQ(S::copy_ctor_calls, 3);
}

TEST(VectorElemDtors) {
  int dtor_calls = 0;
  struct S {
    int &x;
    S(int &x) : x(x) {}
    S(const S &other) : x(other.x) {}
    ~S() { ++x; }
  };

  S s(dtor_calls);
  {
    std::vector<S> v;
    v.push_back(s);
  }
  ASSERT_EQ(dtor_calls, 1);
}

TEST(VectorRange) {
  int x[] = {0, 1, 2};
  bool found[] = {false, false, false};
  std::vector<int> v(x, x + 3);
  for (auto i : v) { found[i] = true; }
  ASSERT_TRUE(found[0]);
  ASSERT_TRUE(found[1]);
  ASSERT_TRUE(found[2]);
}

TEST(VectorMove) {
  size_t heap_used = GetHeapUsed();

  {
    // Empty vector move.
    std::vector<std::unique_ptr<int>> v;
    std::vector<std::unique_ptr<int>> v2(std::move(v));
  }
  ASSERT_EQ(heap_used, GetHeapUsed());

  {
    // Handful of elements.
    std::vector<std::unique_ptr<int>> v;
    v.push_back(std::make_unique<int>(1));
    v.push_back(std::make_unique<int>(2));
    v.push_back(std::make_unique<int>(3));

    std::vector<std::unique_ptr<int>> v2(std::move(v));
    ASSERT_TRUE(v.empty());
    ASSERT_EQ(v2.size(), 3);

    // Accesses to `v` are undefined.
    ASSERT_EQ(*v2[0], 1);
    ASSERT_EQ(*v2[1], 2);
    ASSERT_EQ(*v2[2], 3);
  }
  ASSERT_EQ(heap_used, GetHeapUsed());

  {
    // From a moved reference
    std::vector<std::unique_ptr<int>> v;
    v.push_back(std::make_unique<int>(1));
    v.push_back(std::make_unique<int>(2));
    v.push_back(std::make_unique<int>(3));

    auto &vref = v;

    std::vector<std::unique_ptr<int>> v2(std::move(vref));
    ASSERT_TRUE(v.empty());
    ASSERT_TRUE(vref.empty());
    ASSERT_EQ(v2.size(), 3);

    // Accesses to `v` are undefined.
    ASSERT_EQ(*v2[0], 1);
    ASSERT_EQ(*v2[1], 2);
    ASSERT_EQ(*v2[2], 3);
  }
  ASSERT_EQ(heap_used, GetHeapUsed());
}

TEST(VectorFind) {
  std::vector<int> v = {1, 2, 3};
  for (int i : v) {
    auto it = v.find(i);
    ASSERT_NE(it, v.end());
    ASSERT_EQ(*it, i);
    ASSERT_TRUE(v.contains(i));
  }

  ASSERT_EQ(v.find(4), v.end());
  ASSERT_FALSE(v.contains(4));
}

TEST(VectorErase) {
  {
    // Remove first.
    std::vector<int> v = {1, 2, 3};
    v.erase(v.find(1));
    ASSERT_EQ(v.size(), 2);
    ASSERT_EQ(v[0], 2);
    ASSERT_EQ(v[1], 3);
  }

  {
    // Remove somewhere in the middle.
    std::vector<int> v = {1, 2, 3};
    v.erase(v.find(2));
    ASSERT_EQ(v.size(), 2);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[1], 3);
  }

  {
    // Remove last.
    std::vector<int> v = {1, 2, 3};
    v.erase(v.find(3));
    ASSERT_EQ(v.size(), 2);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[1], 2);

    v.push_back(4);
    ASSERT_EQ(v.size(), 3);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[1], 2);
    ASSERT_EQ(v[2], 4);
  }

  {
    // Remove an element not in the vector.
    std::vector<int> v = {1, 2, 3};
    v.erase(v.end());
    ASSERT_EQ(v.size(), 3);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[1], 2);
    ASSERT_EQ(v[2], 3);
  }
}

TEST_SUITE(VectorSuite) {
  RUN_TEST(PushBack);
  RUN_TEST(VectorRangeCtor);
  RUN_TEST(VectorElemDtors);
  RUN_TEST(VectorRange);
  RUN_TEST(VectorMove);
  RUN_TEST(VectorFind);
  RUN_TEST(VectorErase);
}

TEST(UniqueTest) {
  size_t heapused = GetHeapUsed();
  {
    std::unique_ptr<int> u;
    ASSERT_FALSE(u);

    std::unique_ptr<int> u2(new int(1));
    ASSERT_TRUE(u2);
    ASSERT_EQ(*u2, 1);

    // Assignment
    u = std::move(u2);
    ASSERT_TRUE(u);
    ASSERT_FALSE(u2);

    int *i = u.release();
    ASSERT_FALSE(u);
    ASSERT_EQ(*i, 1);

    // Swapping
    std::unique_ptr<int> u3(i);
    ASSERT_EQ(*u3, 1);
    std::unique_ptr<int> u4(new int(4));
    u3.swap(u4);
    ASSERT_EQ(*u3, 4);
    ASSERT_EQ(*u4, 1);

    // Copy construction.
    std::unique_ptr<int> u5(std::move(u4));
    ASSERT_TRUE(u5);
    ASSERT_FALSE(u4);
    ASSERT_EQ(*u5, 1);
  }

  // Everything should be free'd afterwards.
  ASSERT_EQ(GetHeapUsed(), heapused);
}

TEST(DestructorCalled) {
  bool called = false;
  struct A {
    bool &b_;
    A(bool &b) : b_(b) {}
    ~A() { b_ = true; }
  };

  {
    // At normal scope exit.
    std::unique_ptr<A> a(new A(called));
  }
  ASSERT_TRUE(called);

  called = false;
  {
    std::unique_ptr<A> a(new A(called));
    {
      // Swapping.
      bool junk;
      std::unique_ptr<A> b(new A(junk));

      // Swap the pointers, so once `b` goes out of scope, `a`'s destructor will
      // be called.
      a.swap(b);
    }
    ASSERT_TRUE(called);
  }

  called = false;
  {
    std::unique_ptr<A> a(new A(called));
    {
      // Moving.
      bool junk;
      std::unique_ptr<A> b(new A(junk));

      // Destructor for `b` is called after this move.
      a = std::move(b);
      ASSERT_TRUE(called);
    }
  }

  called = false;
  {
    std::unique_ptr<A> a(new A(called));
    {
      // Copy constructing.
      // Destructor for `a` is called after b exist scope.
      std::unique_ptr<A> b(std::move(a));
    }
    ASSERT_TRUE(called);
  }
}

TEST_SUITE(UniqueSuite) {
  RUN_TEST(UniqueTest);
  RUN_TEST(DestructorCalled);
}

template <typename T>
void CheckInitializerList(const T &l) {
  ASSERT_EQ(l[0], 1);
  ASSERT_EQ(l[1], 2);
  ASSERT_EQ(l[2], 3);
}

TEST(InitializerListTest) {
  // We can only have an implicit list-initialization to initializer_list
  // conversion if the type is explicitly "std::initializer_list".
  auto fn = [](std::initializer_list<int> l) { CheckInitializerList(l); };
  fn({1, 2, 3});

  // Check using directive.
  using std::initializer_list;
  auto fn2 = [](initializer_list<int> l) { CheckInitializerList(l); };
  fn2({1, 2, 3});

  // Check alias.
  auto fn3 = [](toy::InitializerList<int> l) { CheckInitializerList(l); };
  fn3({1, 2, 3});

  toy::InitializerList<int> l({1, 2, 3});
  CheckInitializerList(l);

  toy::InitializerList<int> l2 = {1, 2, 3};
  CheckInitializerList(l2);
}

TEST_SUITE(InitializerListSuite) { RUN_TEST(InitializerListTest); }

TEST(StringTest) {
  std::string s;
  ASSERT_TRUE(s.empty());
  s.push_back('c');
  ASSERT_EQ(s[0], 'c');

  s.push_back('a');
  ASSERT_STREQ(s.c_str(), "ca");
}

TEST(StringConstruction) {
  std::string s("abc");
  ASSERT_EQ(s.size(), 3);
  ASSERT_STREQ(s.c_str(), "abc");

  std::string s2("abc", 2);
  ASSERT_EQ(s2.size(), 2);
  ASSERT_STREQ(s2.c_str(), "ab");

  std::string s3(s.begin() + 1, s.end());
  ASSERT_EQ(s3.size(), 2);
  ASSERT_STREQ(s3.c_str(), "bc");
}

TEST(StringConcat) {
  std::string s("abc");
  std::string s2("def");
  s += s2;
  ASSERT_EQ(s.size(), 6);
  ASSERT_STREQ(s.c_str(), "abcdef");
}

TEST(StringIterator) {
  std::string s("abc");
  size_t i = 0;
  for (auto it = s.begin(); it != s.end(); ++it) {
    ASSERT_EQ(it - s.begin(), i);
    ASSERT_EQ(*it, s[i]);
    ++i;
  }
}

TEST(StringErase) {
  {
    std::string s("abc");
    s.erase(s.find('a'));
    ASSERT_EQ(s.size(), 2);
    ASSERT_EQ(s, "bc");
  }
  {
    std::string s("abc");
    s.erase(s.find('b'));
    ASSERT_EQ(s.size(), 2);
    ASSERT_EQ(s, "ac");
  }
  {
    std::string s("abc");
    s.erase(s.find('c'));
    ASSERT_EQ(s.size(), 2);
    ASSERT_EQ(s, "ab");
  }
  {
    std::string s("abc");
    s.erase(s.find('a'), s.find('a') + 2);
    ASSERT_EQ(s.size(), 1);
    ASSERT_EQ(s, "c");
  }
  {
    std::string s("abc");
    s.erase(s.find('b'), s.find('b') + 2);
    ASSERT_EQ(s.size(), 1);
    ASSERT_EQ(s, "a");
  }
  {
    std::string s("abc");
    s.erase(s.begin(), s.end());
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s, "");
  }
  {
    std::string s("a");
    auto it = s.find('/');
    ASSERT_EQ(it, s.end());

    std::string substr(s.begin(), it + 1);
    ASSERT_EQ(substr.size(), 1);

    s.erase(s.begin(), it + 1);
    ASSERT_EQ(s, "");
  }
  {
    std::string s("a");
    std::string s2(s);

    // The underlying C-strings should be different allocations.
    ASSERT_NE(s.c_str(), s2.c_str());
  }
}

TEST_SUITE(StringSuite) {
  RUN_TEST(StringTest);
  RUN_TEST(StringConstruction);
  RUN_TEST(StringConcat);
  RUN_TEST(StringIterator);
  RUN_TEST(StringErase);
}

TEST(EnumerateIterator) {
  std::vector<int> v({1, 2, 3});
  int i = 0;
  for (auto p : std::ext::Iterable(v)) {
    ++i;
    ASSERT_EQ(p, i);
  }
  ASSERT_EQ(i, 3);

  i = 0;
  for (auto p : std::ext::Enumerate(v)) {
    ASSERT_EQ(p.first, i);
    ++i;
    ASSERT_EQ(p.second, i);
  }
  ASSERT_EQ(i, 3);
}

TEST_SUITE(Iterators) { RUN_TEST(EnumerateIterator); }

[[maybe_unused]] float FreeFunc(int a, char b) {
  return static_cast<float>(a + b);
}

TEST(FunctionReturnType) {
  {
    using Traits = std::ext::FunctionTraits<decltype(FreeFunc)>;
    static_assert(std::is_same<Traits::return_type, float>::value, "");
    static_assert(Traits::num_args == 2, "");
    static_assert(std::is_same<Traits::argument<0>::type, int>::value, "");
    static_assert(std::is_same<Traits::argument<1>::type, char>::value, "");
  }

  struct A {
    int func(char) { return 0; }
  };
  {
    using Traits = std::ext::FunctionTraits<decltype(&A::func)>;
    static_assert(std::is_same<Traits::return_type, int>::value, "");
    static_assert(Traits::num_args == 2, "");
    static_assert(std::is_same<Traits::argument<0>::type, A &>::value, "");
    static_assert(std::is_same<Traits::argument<1>::type, char>::value, "");
  }
}

TEST_SUITE(TypeTraits) { RUN_TEST(FunctionReturnType); }

TEST(BitVectorTest) {
  BitVector v;
  ASSERT_TRUE(v.empty());

  for (auto i = 0; i < 32; ++i) {
    v.push_back(1);
    ASSERT_EQ(v.size(), i + 1);
    ASSERT_TRUE(v.get(i));
    ASSERT_EQ((UINT64_C(1) << (i + 1)) - 1, v.getAs<uint64_t>());
  }

  ASSERT_EQ(v.size(), 32);
  ASSERT_EQ(v.getAs<uint32_t>(), UINT32_MAX);

  v.push_back(0);
  ASSERT_EQ(v.getBack(), 0);
  ASSERT_EQ(v.get(32), 0);
  ASSERT_EQ(v.get(31), 1);
  ASSERT_EQ(v.size(), 33);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT32_MAX);
  v.set(32, 0);
  ASSERT_EQ(v.size(), 33);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT32_MAX);
  v.set(32, 1);
  ASSERT_EQ(v.getAs<uint64_t>(), (static_cast<uint64_t>(UINT32_MAX) << 1) + 1);

  for (auto i = 33; i < 63; ++i) {
    v.push_back(1);
    ASSERT_EQ(v.size(), i + 1);
    ASSERT_TRUE(v.get(i));
    ASSERT_EQ((UINT64_C(1) << (i + 1)) - 1, v.getAs<uint64_t>());
  }

  ASSERT_EQ(v.size(), 63);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_MAX >> 1);

  v.push_back(1);
  ASSERT_EQ(v.size(), 64);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_MAX);

  // Cannot fit into 64 bits anymore.
  v.push_back(1);
  ASSERT_EQ(v.size(), 65);

  for (int i = 64; i >= 0; --i) {
    ASSERT_TRUE(v.get(i));
    v.set(i, 0);
    ASSERT_FALSE(v.get(i));
  }

  ASSERT_EQ(v.size(), 65);

  v.pop_back();
  ASSERT_EQ(v.size(), 64);
  ASSERT_EQ(v.getAs<uint64_t>(), 0);
  v.pop_back();
  v.push_back(1);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_C(1) << 63);
}

TEST_SUITE(BitVectorSuite) { RUN_TEST(BitVectorTest); }

TEST(TupleTest) {
  std::tuple<int, char, short> t = {1, 2, 3};
  static_assert(std::tuple_size<decltype(t)>::value == 3);
  ASSERT_EQ(std::get<0>(t), 1);
  ASSERT_EQ(std::get<1>(t), 2);
  ASSERT_EQ(std::get<2>(t), 3);
}

TEST_SUITE(TupleSuite) { RUN_TEST(TupleTest); }

TEST(VFSRootDir) {
  size_t heap_used = GetHeapUsed();
  {
    Directory root;
    ASSERT_EQ(root.NumFiles(), 0);
    ASSERT_TRUE(root.empty());

    root.mkdir("a");
    ASSERT_EQ(root.NumFiles(), 1);
    ASSERT_FALSE(root.empty());

    // Add an existing file.
    root.mkdir("a");
    ASSERT_EQ(root.NumFiles(), 1);
    ASSERT_TRUE(root.hasDir("a"));

    // Trailing path separator.
    root.mkdir("a/");
    ASSERT_EQ(root.NumFiles(), 1);

    root.mkdir("b/");
    ASSERT_EQ(root.NumFiles(), 2);
    ASSERT_TRUE(root.hasDir("b"));

    ASSERT_FALSE(root.hasDir("  c/  "));
    root.mkdir("  c/  ");
    ASSERT_EQ(root.NumFiles(), 3);
    ASSERT_TRUE(root.hasDir("c"));
    ASSERT_TRUE(root.hasDir(" c/ "));
    ASSERT_TRUE(root.hasDir(" c "));

    {
      Directory &dir = root.mkdir("a/b");
      ASSERT_EQ(root.NumFiles(), 3);
      ASSERT_TRUE(root.hasDir("a"));
      ASSERT_EQ(root.getDir("a")->NumFiles(), 1);
      ASSERT_EQ(root.getDir("a")->getDir("b"), &dir);
      ASSERT_TRUE(dir.empty());
    }

    {
      Directory &dir = root.mkdir("a/b/c/d/");
      ASSERT_EQ(root.NumFiles(), 3);
      ASSERT_EQ(root.getDir("a")->NumFiles(), 1);
      ASSERT_EQ(root.getDir("a")->getDir("b")->getDir("c")->getDir("d"), &dir);
      ASSERT_TRUE(dir.empty());
    }

    {
      File &file = root.mkfile("d");
      ASSERT_EQ(root.NumFiles(), 4);
      ASSERT_EQ(root.getFile("d"), &file);
      ASSERT_TRUE(root.getFile("d")->empty());

      // Writing
      const char str[] = "abcd";
      file.Write(str, strlen(str));
      ASSERT_EQ(root.getFile("d")->getSize(), 4);
      ASSERT_EQ(
          memcmp(root.getFile("d")->getContents().data(), str, strlen(str)), 0);
    }

    {
      File &file = root.mkfile("a/a");
      ASSERT_EQ(root.getDir("a")->NumFiles(), 2);
      ASSERT_EQ(root.getDir("a")->getFile("a"), &file);
    }

    {
      File &file = root.mkfile("a/b/c/d/e");
      ASSERT_EQ(
          root.getDir("a")->getDir("b")->getDir("c")->getDir("d")->getFile("e"),
          &file);
    }

    {
      const char path[] = "initrd_files/test_user_program.bin";
      File &file = root.mkfile(path);
      ASSERT_TRUE(root.hasDir("initrd_files"));
      ASSERT_TRUE(
          root.getDir("initrd_files")->hasFile("test_user_program.bin"));
      ASSERT_EQ(root.getDir("initrd_files")->getFile("test_user_program.bin"),
                &file);
    }
  }
  ASSERT_EQ(heap_used, GetHeapUsed());
}

TEST_SUITE(VFS) { RUN_TEST(VFSRootDir); }

TEST(RTTICasts) {
  struct A {
    enum A_Kind {
      // clang-format off
      DEF_KIND_FIRST(A)
        DEF_KIND_FIRST(B)
          DEF_KIND_LEAF(D)
          DEF_KIND_FIRST(E)
            DEF_KIND_LEAF(F)
          DEF_KIND_LAST(E)
        DEF_KIND_LAST(B)

        DEF_KIND_FIRST(C)
        // Note that we should also check for E in C.
        DEF_KIND_LAST(C)
      DEF_KIND_LAST(A)
      // clang-format on
    };

    const A_Kind kind_;
    A_Kind getKind() const { return kind_; }

    A() : kind_(RTTI_KIND(A)) {}
    A(A_Kind kind) : kind_(kind) {}

    int a = 1;
    virtual int func() { return a; }

    DEFINE_CLASSOF(A, A)
  };

  struct B : public virtual A {
    int b = 2;
    int func() override { return b; }
    B() : B(RTTI_KIND(B)) {}
    B(A_Kind kind) : A(kind) {}

    DEFINE_CLASSOF(A, B)
  };

  struct C : public virtual A {
    int c = 3;
    int func() override { return c; }

    C() : C(RTTI_KIND(C)) {}
    C(A_Kind kind) : A(kind) {}

    // We also need to do an extra check for E in C because the kind for E is
    // actually defined within B.
    DEFINE_CLASSOF2(A, C, E)
  };

  struct D : public B {
    int d = 4;
    int func() override { return d; }

    D() : D(RTTI_KIND(D)) {}
    D(A_Kind kind) : A(kind), B(kind) {}
    DEFINE_CLASSOF(A, D)
  };

  struct E : public B, public C {
    int e = 5;
    int func() override { return e; }

    E() : E(RTTI_KIND(E)) {}
    E(A_Kind kind) : A(kind), B(kind), C(kind) {}

    DEFINE_CLASSOF(A, E)
  };

  struct F : public E {
    int f = 6;
    int func() override { return f; }

    F() : A(RTTI_KIND(F)), E(RTTI_KIND(F)) {}
    DEFINE_CLASSOF(A, F)
  };

  A a;
  B b;
  C c;
  D d;
  E e;
  F f;

  A *a_b = &b;
  A *a_c = &c;
  A *a_d = &d;
  A *a_e = &e;
  A *a_f = &f;

  // A
  ASSERT_TRUE(isa<A>(&a));
  ASSERT_FALSE(isa<B>(&a));
  ASSERT_FALSE(isa<C>(&a));
  ASSERT_FALSE(isa<D>(&a));
  ASSERT_FALSE(isa<E>(&a));
  ASSERT_FALSE(isa<F>(&a));

  ASSERT_TRUE(isa<A>(a_b));
  ASSERT_TRUE(isa<A>(a_c));
  ASSERT_TRUE(isa<A>(a_d));
  ASSERT_TRUE(isa<A>(a_e));
  ASSERT_TRUE(isa<A>(a_f));

  // Note that we can't cast parents to virtual children, so we can't assert
  // something like `dyn_cast<B>(&a) == nullptr`.
  ASSERT_EQ_3WAY(dyn_cast<A>(&a), cast<A>(&a), &a);
  ASSERT_EQ_3WAY(dyn_cast<A>(a_b), cast<A>(a_b), &b);
  ASSERT_EQ_3WAY(dyn_cast<A>(a_c), cast<A>(a_c), &c);
  ASSERT_EQ_3WAY(dyn_cast<A>(a_d), cast<A>(a_d), &d);
  ASSERT_EQ_3WAY(dyn_cast<A>(a_e), cast<A>(a_e), &e);
  ASSERT_EQ_3WAY(dyn_cast<A>(a_f), cast<A>(a_f), &f);

  // B
  ASSERT_TRUE(isa<A>(&b));
  ASSERT_TRUE(isa<B>(&b));
  ASSERT_FALSE(isa<C>(&b));
  ASSERT_FALSE(isa<D>(&b));
  ASSERT_FALSE(isa<E>(&b));
  ASSERT_FALSE(isa<F>(&b));

  ASSERT_TRUE(isa<A>(a_b));
  ASSERT_TRUE(isa<B>(a_b));
  ASSERT_FALSE(isa<C>(a_b));
  ASSERT_FALSE(isa<D>(a_b));
  ASSERT_FALSE(isa<E>(a_b));
  ASSERT_FALSE(isa<F>(a_b));

  ASSERT_EQ_3WAY(dyn_cast<A>(&b), cast<A>(&b), &b);
  ASSERT_EQ_3WAY(dyn_cast<B>(&b), cast<B>(&b), &b);
  ASSERT_EQ(dyn_cast<C>(&b), nullptr);
  ASSERT_EQ(dyn_cast<D>(&b), nullptr);
  ASSERT_EQ(dyn_cast<E>(&b), nullptr);
  ASSERT_EQ(dyn_cast<F>(&b), nullptr);

  // Note that we can't cast parents to virtual children, so we can't assert
  // something like `dyn_cast<B>(&a) == nullptr`. This follows sute for the
  // remaining a_* variables. This is a matter of the language and not this RTTI
  // feature.
  ASSERT_EQ_3WAY(dyn_cast<A>(a_b), cast<A>(a_b), a_b);

  // C
  ASSERT_TRUE(isa<A>(&c));
  ASSERT_FALSE(isa<B>(&c));
  ASSERT_TRUE(isa<C>(&c));
  ASSERT_FALSE(isa<D>(&c));
  ASSERT_FALSE(isa<E>(&c));
  ASSERT_FALSE(isa<F>(&c));

  ASSERT_TRUE(isa<A>(a_c));
  ASSERT_FALSE(isa<B>(a_c));
  ASSERT_TRUE(isa<C>(a_c));
  ASSERT_FALSE(isa<D>(a_c));
  ASSERT_FALSE(isa<E>(a_c));
  ASSERT_FALSE(isa<F>(a_c));

  ASSERT_EQ_3WAY(dyn_cast<A>(&c), cast<A>(&c), &c);
  ASSERT_EQ(dyn_cast<B>(&c), nullptr);
  ASSERT_EQ_3WAY(dyn_cast<C>(&c), cast<C>(&c), &c);
  ASSERT_EQ(dyn_cast<D>(&c), nullptr);
  ASSERT_EQ(dyn_cast<E>(&c), nullptr);
  ASSERT_EQ(dyn_cast<F>(&c), nullptr);

  // D
  ASSERT_TRUE(isa<A>(&d));
  ASSERT_TRUE(isa<B>(&d));
  ASSERT_FALSE(isa<C>(&d));
  ASSERT_TRUE(isa<D>(&d));
  ASSERT_FALSE(isa<E>(&d));
  ASSERT_FALSE(isa<F>(&d));

  ASSERT_TRUE(isa<A>(a_d));
  ASSERT_TRUE(isa<B>(a_d));
  ASSERT_FALSE(isa<C>(a_d));
  ASSERT_TRUE(isa<D>(a_d));
  ASSERT_FALSE(isa<E>(a_d));
  ASSERT_FALSE(isa<F>(a_d));

  ASSERT_EQ_3WAY(dyn_cast<A>(&d), cast<A>(&d), &d);
  ASSERT_EQ_3WAY(dyn_cast<B>(&d), cast<B>(&d), &d);
  ASSERT_EQ(dyn_cast<C>(&d), nullptr);
  ASSERT_EQ_3WAY(dyn_cast<D>(&d), cast<D>(&d), &d);
  ASSERT_EQ(dyn_cast<E>(&d), nullptr);
  ASSERT_EQ(dyn_cast<F>(&d), nullptr);

  // E
  ASSERT_TRUE(isa<A>(&e));
  ASSERT_TRUE(isa<B>(&e));
  ASSERT_TRUE(isa<C>(&e));
  ASSERT_FALSE(isa<D>(&e));
  ASSERT_TRUE(isa<E>(&e));
  ASSERT_FALSE(isa<F>(&e));

  ASSERT_TRUE(isa<A>(a_e));
  ASSERT_TRUE(isa<B>(a_e));
  ASSERT_TRUE(isa<C>(a_e));
  ASSERT_FALSE(isa<D>(a_e));
  ASSERT_TRUE(isa<E>(a_e));
  ASSERT_FALSE(isa<F>(a_e));

  ASSERT_EQ_3WAY(dyn_cast<A>(&e), cast<A>(&e), &e);
  ASSERT_EQ_3WAY(dyn_cast<B>(&e), cast<B>(&e), &e);
  ASSERT_EQ_3WAY(dyn_cast<C>(&e), cast<C>(&e), &e);
  ASSERT_EQ(dyn_cast<D>(&e), nullptr);
  ASSERT_EQ_3WAY(dyn_cast<E>(&e), cast<E>(&e), &e);
  ASSERT_EQ(dyn_cast<F>(&e), nullptr);

  // F
  ASSERT_TRUE(isa<A>(&f));
  ASSERT_TRUE(isa<B>(&f));
  ASSERT_TRUE(isa<C>(&f));
  ASSERT_FALSE(isa<D>(&f));
  ASSERT_TRUE(isa<E>(&f));
  ASSERT_TRUE(isa<F>(&f));

  ASSERT_TRUE(isa<A>(a_f));
  ASSERT_TRUE(isa<B>(a_f));
  ASSERT_TRUE(isa<C>(a_f));
  ASSERT_FALSE(isa<D>(a_f));
  ASSERT_TRUE(isa<E>(a_f));
  ASSERT_TRUE(isa<F>(a_f));

  ASSERT_EQ_3WAY(dyn_cast<A>(&f), cast<A>(&f), &f);
  ASSERT_EQ_3WAY(dyn_cast<B>(&f), cast<B>(&f), &f);
  ASSERT_EQ_3WAY(dyn_cast<C>(&f), cast<C>(&f), &f);
  ASSERT_EQ(dyn_cast<D>(&f), nullptr);
  ASSERT_EQ_3WAY(dyn_cast<E>(&f), cast<E>(&f), &f);
  ASSERT_EQ_3WAY(dyn_cast<F>(&f), cast<F>(&f), &f);
}

TEST_SUITE(RTTI) { RUN_TEST(RTTICasts); }

}  // namespace

int main() {
  printf("=== RUNTESTS ===\n\n");

  test::TestingFramework tests;
  tests.RunSuite(MathFunctions);
  tests.RunSuite(CString);
  tests.RunSuite(Printing);
  tests.RunSuite(Malloc);
  tests.RunSuite(Calloc);
  tests.RunSuite(CppLangFeatures);
  tests.RunSuite(VectorSuite);
  tests.RunSuite(UniqueSuite);
  tests.RunSuite(InitializerListSuite);
  tests.RunSuite(StringSuite);
  tests.RunSuite(Iterators);
  tests.RunSuite(TypeTraits);
  tests.RunSuite(BitVectorSuite);
  tests.RunSuite(TupleSuite);
  tests.RunSuite(VFS);
  tests.RunSuite(RTTI);

  return 0;
}
