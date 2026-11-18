#include "core/log/StallTrace.hpp"
#include "core/log/Log.hpp"

#include <chrono>
#include <vector>

namespace LunaDash::StallTrace {
namespace {
// A dispatch this long shows as a visible hitch; sections shorter than the
// floor are not worth naming.
constexpr long long kStallUs = 150000;
constexpr long long kSectionFloorUs = 10000;

long long nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct Slow {
  const char *name;
  std::string detail;
  long long us;
};

thread_local long long dispatchStartedUs = 0;
thread_local std::vector<Slow> slow;
} // namespace

Section::Section(const char *name, std::string_view detail)
    : name_(name), detail_(detail), startedUs_(nowUs()) {}

Section::~Section() {
  const long long elapsed = nowUs() - startedUs_;
  if (elapsed >= kSectionFloorUs && slow.size() < 32)
    slow.push_back({name_, std::move(detail_), elapsed});
}

void beginDispatch() {
  dispatchStartedUs = nowUs();
  slow.clear();
}

void endDispatch(const char *source) {
  const long long elapsed = nowUs() - dispatchStartedUs;
  if (elapsed >= kStallUs) {
    std::string sections;
    for (const auto &entry : slow) {
      sections += sections.empty() ? "" : ", ";
      sections += entry.name;
      if (!entry.detail.empty())
        sections += " " + entry.detail;
      sections += " " + std::to_string(entry.us / 1000) + " ms";
    }
    logWarning("LunaDash stall: %lld ms in one %s dispatch (%s)", elapsed / 1000,
               source, sections.empty() ? "no traced section" : sections.c_str());
  }
  slow.clear();
}

} // namespace LunaDash::StallTrace
