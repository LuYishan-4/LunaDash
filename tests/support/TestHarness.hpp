#pragma once

// Minimal xUnit-style harness for LunaDash's native C++ tests. A test fixture
// is a plain class whose test methods are registered with LUDASH_TEST_MAIN.
// Optional initTestCase/cleanupTestCase/init/cleanup methods are recognised by
// name. Failures throw out of the running test, so later tests still run.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace LunaDash::Test {

struct Failure {
  std::string message;
};
struct Skipped {
  std::string reason;
};

template <typename T, typename = void> struct Printable : std::false_type {};
template <typename T>
struct Printable<T, std::void_t<decltype(std::declval<std::ostream &>()
                                         << std::declval<const T &>())>>
    : std::true_type {};

template <typename T> std::string describe(const T &value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (Printable<T>::value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
  } else {
    return "<value>";
  }
}

[[noreturn]] inline void fail(const std::string &message, const char *file,
                              int line) {
  std::ostringstream stream;
  stream << file << ':' << line << ": " << message;
  throw Failure{stream.str()};
}

inline void verify(bool condition, const char *expression, const char *file,
                   int line) {
  if (!condition)
    fail(std::string("VERIFY(") + expression + ") failed", file, line);
}

template <typename Actual, typename Expected>
void compare(const Actual &actual, const Expected &expected,
             const char *actualExpression, const char *expectedExpression,
             const char *file, int line) {
  if (actual == expected)
    return;
  fail(std::string("COMPARE(") + actualExpression + ", " + expectedExpression +
           ") failed\n   Actual:   " + describe(actual) +
           "\n   Expected: " + describe(expected),
       file, line);
}

inline bool fuzzyEqual(double a, double b) {
  const double scale = std::max({1.0, a < 0 ? -a : a, b < 0 ? -b : b});
  const double difference = a > b ? a - b : b - a;
  return difference <= scale * 1e-9;
}

// Repeatedly evaluates condition while pump() advances asynchronous work.
inline bool waitUntil(const std::function<bool()> &condition,
                      int timeoutMilliseconds,
                      const std::function<void()> &pump = {}) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMilliseconds);
  while (!condition()) {
    if (std::chrono::steady_clock::now() >= deadline)
      return condition();
    if (pump)
      pump();
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

template <typename Fixture> struct Case {
  const char *name;
  void (Fixture::*method)();
};

template <typename Fixture>
int run(const char *suite, std::initializer_list<Case<Fixture>> cases,
        int argc = 0, char **argv = nullptr) {
  std::vector<std::string> selected;
  for (int index = 1; index < argc; ++index)
    selected.emplace_back(argv[index]);
  const auto wanted = [&](const char *name) {
    if (selected.empty())
      return true;
    for (const auto &entry : selected)
      if (entry == name)
        return true;
    return false;
  };
  const auto find = [&](const char *name) -> void (Fixture::*)() {
    for (const auto &entry : cases)
      if (std::strcmp(entry.name, name) == 0)
        return entry.method;
    return nullptr;
  };
  const auto initTestCase = find("initTestCase");
  const auto cleanupTestCase = find("cleanupTestCase");
  const auto init = find("init");
  const auto cleanup = find("cleanup");
  const auto special = [](const char *name) {
    return std::strcmp(name, "initTestCase") == 0 ||
           std::strcmp(name, "cleanupTestCase") == 0 ||
           std::strcmp(name, "init") == 0 || std::strcmp(name, "cleanup") == 0;
  };

  int failures = 0;
  int passed = 0;
  int skipped = 0;
  std::cout << "********* Start testing of " << suite << " *********\n";
  Fixture fixture;
  const auto invoke = [&](const char *name, void (Fixture::*method)()) {
    try {
      (fixture.*method)();
      return 0;
    } catch (const Skipped &skip) {
      std::cout << "SKIP   : " << suite << "::" << name << "() "
                << skip.reason << '\n';
      return 1;
    } catch (const Failure &failure) {
      std::cout << "FAIL!  : " << suite << "::" << name << "() "
                << failure.message << '\n';
    } catch (const std::exception &exception) {
      std::cout << "FAIL!  : " << suite << "::" << name
                << "() threw: " << exception.what() << '\n';
    } catch (...) {
      std::cout << "FAIL!  : " << suite << "::" << name
                << "() threw an unknown exception\n";
    }
    return -1;
  };
  if (initTestCase) {
    const int result = invoke("initTestCase", initTestCase);
    if (result < 0) {
      std::cout << "Totals: 0 passed, 1 failed\n";
      return 1;
    }
    if (result > 0) {
      std::cout << "Totals: 0 passed, 0 failed, all skipped\n";
      return 0;
    }
  }
  for (const auto &entry : cases) {
    if (special(entry.name) || !wanted(entry.name))
      continue;
    if (init && invoke("init", init) < 0) {
      ++failures;
      continue;
    }
    const int result = invoke(entry.name, entry.method);
    if (cleanup && invoke("cleanup", cleanup) < 0 && result == 0) {
      ++failures;
      continue;
    }
    if (result == 0) {
      ++passed;
      std::cout << "PASS   : " << suite << "::" << entry.name << "()\n";
    } else if (result > 0) {
      ++skipped;
    } else {
      ++failures;
    }
  }
  if (cleanupTestCase && invoke("cleanupTestCase", cleanupTestCase) < 0)
    ++failures;
  std::cout << "Totals: " << passed << " passed, " << failures << " failed, "
            << skipped << " skipped\n"
            << "********* Finished testing of " << suite << " *********\n";
  return failures == 0 ? 0 : 1;
}

} // namespace LunaDash::Test

#define VERIFY(condition)                                                      \
  ::LunaDash::Test::verify(static_cast<bool>(condition), #condition, __FILE__, \
                           __LINE__)
#define VERIFY2(condition, message)                                            \
  do {                                                                         \
    if (!(condition))                                                          \
      ::LunaDash::Test::fail(std::string("VERIFY(" #condition "): ") +         \
                                 std::string(message),                         \
                             __FILE__, __LINE__);                              \
  } while (false)
#define COMPARE(actual, expected)                                              \
  ::LunaDash::Test::compare((actual), (expected), #actual, #expected,          \
                            __FILE__, __LINE__)
#define FUZZY_COMPARE(actual, expected)                                        \
  ::LunaDash::Test::verify(                                                    \
      ::LunaDash::Test::fuzzyEqual((actual), (expected)),                      \
      "fuzzy " #actual " == " #expected, __FILE__, __LINE__)
#define SKIP(reason) throw ::LunaDash::Test::Skipped{reason}
#define FAIL(message) ::LunaDash::Test::fail(message, __FILE__, __LINE__)
#define TRY_VERIFY_WITH_TIMEOUT(condition, timeout)                            \
  ::LunaDash::Test::verify(                                                    \
      ::LunaDash::Test::waitUntil([&] { return static_cast<bool>(condition); }, \
                                  (timeout)),                                  \
      #condition, __FILE__, __LINE__)
