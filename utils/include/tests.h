#ifndef TESTS_H_
#define TESTS_H_

/**
 * Small testing framework.
 */

#ifndef STR
#define __STR(s) STR(s)
#define STR(s) #s
#endif

namespace test {

using SetupFunc = void (*)();
using TeardownFunc = void (*)();

SetupFunc TestingSetup = nullptr;
TeardownFunc TestingTeardown = nullptr;
int32_t NumFailures;
bool TestFailed = false;

#define TEST(Test) [[maybe_unused]] static void Test()
#define TEST_SUITE(Suite) [[maybe_unused]] static void Suite()
#define SETUP(Setup) \
  { test::TestingSetup = Setup; }
#define RUN_TEST(TestName)                                 \
  {                                                        \
    if (test::TestingSetup) test::TestingSetup();          \
    test::TestFailed = false;                              \
    PRINT(STR(TestName) " ... ");                          \
    TestName();                                            \
    PRINT("{}\n", test::TestFailed ? "FAILED" : "PASSED"); \
    if (test::TestingTeardown) test::TestingTeardown();    \
  }

#define ASSERT_TRUE(val1)                                               \
  {                                                                     \
    if (!test::AssertTrue(val1, STR(val1), __FILE__, __LINE__)) return; \
  }

#define ASSERT_FALSE(val1)                                               \
  {                                                                      \
    if (!test::AssertFalse(val1, STR(val1), __FILE__, __LINE__)) return; \
  }

#define ASSERT_STREQ(s1, s2)                                                 \
  {                                                                          \
    if (!test::AssertStrEqual(s1, s2, STR(s1), STR(s2), __FILE__, __LINE__)) \
      return;                                                                \
  }

#define ASSERT_EQ(val1, val2)                                          \
  {                                                                    \
    if (!test::AssertEqual(val1, val2, STR(val1), STR(val2), __FILE__, \
                           __LINE__))                                  \
      return;                                                          \
  }
#define ASSERT_EQ_3WAY(val1, val2, val3)                               \
  {                                                                    \
    if (!test::AssertEqual(val1, val2, STR(val1), STR(val2), __FILE__, \
                           __LINE__))                                  \
      return;                                                          \
    if (!test::AssertEqual(val1, val3, STR(val1), STR(val3), __FILE__, \
                           __LINE__))                                  \
      return;                                                          \
    if (!test::AssertEqual(val2, val3, STR(val2), STR(val3), __FILE__, \
                           __LINE__))                                  \
      return;                                                          \
  }

#define ASSERT_NE(val1, val2)                                             \
  {                                                                       \
    if (!test::AssertNotEqual(val1, val2, STR(val1), STR(val2), __FILE__, \
                              __LINE__))                                  \
      return;                                                             \
  }

#define ASSERT_GE(val1, val2)                                             \
  {                                                                       \
    if (!test::AssertGreaterThanOrEqual(val1, val2, STR(val1), STR(val2), \
                                        __FILE__, __LINE__))              \
      return;                                                             \
  }

class TestingFramework {
 public:
  TestingFramework() {
    NumFailures = 0;
    PRINT("\nRunning tests...\n");
  }

  ~TestingFramework() {
    if (NumFailures)
      PRINT("{} tests failed\n\n", NumFailures);
    else
      PRINT("All tests passed!\n\n");
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
  PRINT("Strings are not equal {}:{}\n", file, line);
  PRINT("Found `{}` which is:\n", found_expr);
  PRINT("  {}\n\n", found);
  PRINT("Expected `{}` which is:\n", expected_expr);
  PRINT("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1>
inline bool AssertTrue(const T1 &found, const char *found_expr,
                       const char *file, int line) {
  if (found) return true;
  PRINT("Expected true value at {}:{}\n", file, line);
  PRINT("Found `{}` which is false\n", found_expr);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1>
inline bool AssertFalse(const T1 &found, const char *found_expr,
                        const char *file, int line) {
  if (!found) return true;
  PRINT("Expected false value at {}:{}\n", file, line);
  PRINT("Found `{}` which is true\n", found_expr);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
inline bool AssertEqual(const T1 &found, const T2 &expected,
                        const char *found_expr, const char *expected_expr,
                        const char *file, int line) {
  if (found == expected) return true;
  PRINT("Values are not equal {}:{}\n", file, line);
  PRINT("Found `{}` which is:\n", found_expr);
  PRINT("  {}\n\n", found);
  PRINT("Expected `{}` which is:\n", expected_expr);
  PRINT("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
inline bool AssertNotEqual(const T1 &found, const T2 &expected,
                           const char *found_expr, const char *expected_expr,
                           const char *file, int line) {
  if (found != expected) return true;
  PRINT("Values are equal {}:{}\n", file, line);
  PRINT("Found `{}` which is:\n", found_expr);
  PRINT("  {}\n\n", found);
  PRINT("Received `{}` which is:\n", expected_expr);
  PRINT("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
inline bool AssertGreaterThanOrEqual(const T1 &found, const T2 &expected,
                                     const char *found_expr,
                                     const char *expected_expr,
                                     const char *file, int line) {
  if (found >= expected) return true;
  PRINT("Found value is less than the expected {}:{}\n", file, line);
  PRINT("Found `{}` which is:\n", found_expr);
  PRINT("  {}\n\n", found);
  PRINT("Expected `{}` which is:\n", expected_expr);
  PRINT("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

}  // namespace test

#endif
