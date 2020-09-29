#ifndef TESTS_H_
#define TESTS_H_

#include <Terminal.h>

/**
 * Small testing framework for this kernel.
 */

namespace {

using SetupFunc = void (*)();
using TeardownFunc = void (*)();

SetupFunc TestingSetup = nullptr;
TeardownFunc TestingTeardown = nullptr;
int32_t NumFailures;
bool TestFailed = false;

#define TEST(Test) static void Test()
#define TEST_SUITE(Suite) static void Suite()
#define SETUP(Setup) \
  { TestingSetup = Setup; }
#define RUN_TEST(TestName)                                      \
  {                                                             \
    if (TestingSetup) TestingSetup();                           \
    TestFailed = false;                                         \
    terminal::Write(STR(TestName) " ... ");                     \
    TestName();                                                 \
    terminal::WriteF("{}\n", TestFailed ? "FAILED" : "PASSED"); \
    if (TestingTeardown) TestingTeardown();                     \
  }

#define ASSERT_TRUE(val1)                                         \
  {                                                               \
    if (!AssertTrue(val1, STR(val1), __FILE__, __LINE__)) return; \
  }

#define ASSERT_FALSE(val1)                                         \
  {                                                                \
    if (!AssertFalse(val1, STR(val1), __FILE__, __LINE__)) return; \
  }

#define ASSERT_STREQ(s1, s2)                                                   \
  {                                                                            \
    if (!AssertStrEqual(s1, s2, STR(s1), STR(s2), __FILE__, __LINE__)) return; \
  }

#define ASSERT_EQ(val1, val2)                                               \
  {                                                                         \
    if (!AssertEqual(val1, val2, STR(val1), STR(val2), __FILE__, __LINE__)) \
      return;                                                               \
  }

#define ASSERT_NE(val1, val2)                                                  \
  {                                                                            \
    if (!AssertNotEqual(val1, val2, STR(val1), STR(val2), __FILE__, __LINE__)) \
      return;                                                                  \
  }

class TestingFramework {
 public:
  TestingFramework() {
    NumFailures = 0;
    terminal::Write("\nRunning tests...\n");
  }

  ~TestingFramework() {
    if (NumFailures)
      terminal::WriteF("{} tests failed\n\n", NumFailures);
    else
      terminal::Write("All tests passed!\n\n");
    while (NumFailures) {}
  }

  using TestSuite = void (*)();

  void RunSuite(TestSuite suite) {
    TestingSetup = nullptr;
    TestingTeardown = nullptr;
    suite();
  }
};

inline bool AssertStrEqual(const char *found, const char *expected,
                           const char *found_expr, const char *expected_expr,
                           const char *file, int line) {
  if (strcmp(found, expected) == 0) return true;
  terminal::WriteF("Strings are not equal {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is:\n", found_expr);
  terminal::WriteF("  {}\n\n", found);
  terminal::WriteF("Expected `{}` which is:\n", expected_expr);
  terminal::WriteF("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1>
bool AssertTrue(const T1 &found, const char *found_expr, const char *file,
                int line) {
  if (found) return true;
  terminal::WriteF("Expected true value at {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is false\n", found_expr);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1>
bool AssertFalse(const T1 &found, const char *found_expr, const char *file,
                 int line) {
  if (!found) return true;
  terminal::WriteF("Expected false value at {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is true\n", found_expr);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
bool AssertEqual(const T1 &found, const T2 &expected, const char *found_expr,
                 const char *expected_expr, const char *file, int line) {
  if (found == expected) return true;
  terminal::WriteF("Values are not equal {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is:\n", found_expr);
  terminal::WriteF("  {}\n\n", found);
  terminal::WriteF("Expected `{}` which is:\n", expected_expr);
  terminal::WriteF("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
bool AssertNotEqual(const T1 &found, const T2 &expected, const char *found_expr,
                    const char *expected_expr, const char *file, int line) {
  if (found != expected) return true;
  terminal::WriteF("Values are equal {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is:\n", found_expr);
  terminal::WriteF("  {}\n\n", found);
  terminal::WriteF("Received `{}` which is:\n", expected_expr);
  terminal::WriteF("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

}  // namespace

#endif
