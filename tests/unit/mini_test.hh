// Minimal self-contained test helpers (no external framework required).
#ifndef XRCL_MINI_TEST_HH
#define XRCL_MINI_TEST_HH

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace mt {

struct Case {
  std::string name;
  std::function<void()> fn;
};
inline std::vector<Case>& Registry()
{
  static std::vector<Case> r;
  return r;
}
inline int& Failures()
{
  static int f = 0;
  return f;
}
struct Reg {
  Reg(const char* n, std::function<void()> f) { Registry().push_back({n, std::move(f)}); }
};

inline void Check(bool ok, const std::string& what, const char* file, int line)
{
  if (!ok) {
    ++Failures();
    std::cout << "    FAIL " << file << ":" << line << "  " << what << "\n";
  }
}

inline int RunAll()
{
  int failedCases = 0;
  for (auto& c : Registry()) {
    const int before = Failures();
    try {
      c.fn();
    }
    catch (const std::exception& e) {
      ++Failures();
      std::cout << "    EXCEPTION " << e.what() << "\n";
    }
    const bool ok = Failures() == before;
    failedCases += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << c.name << "\n";
  }
  std::cout << (Registry().size() - failedCases) << "/" << Registry().size() << " unit test cases passed\n";
  return failedCases == 0 ? 0 : 1;
}

}  // namespace mt

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)
#define TEST_CASE(name)                                          \
  static void MT_CAT(mt_fn_, __LINE__)();                        \
  static mt::Reg MT_CAT(mt_reg_, __LINE__)(name, MT_CAT(mt_fn_, __LINE__)); \
  static void MT_CAT(mt_fn_, __LINE__)()
#define CHECK(cond) mt::Check((cond), #cond, __FILE__, __LINE__)
#define CHECK_MSG(cond, msg) mt::Check((cond), std::string(#cond) + " | " + (msg), __FILE__, __LINE__)

#endif
