#pragma once

#include <string>
#include <string_view>

namespace LunaDash::StallTrace {

// Times a stretch of main-thread work. When one event-loop dispatch stalls
// (everything on screen freezes meanwhile), the log names the sections inside
// it that took long, so a stall seen on a real machine can be traced.
class Section {
public:
  explicit Section(const char *name, std::string_view detail = {});
  ~Section();
  Section(const Section &) = delete;
  Section &operator=(const Section &) = delete;

private:
  const char *name_;
  std::string detail_;
  long long startedUs_;
};

// Around each dispatch of the event loop.
void beginDispatch();
void endDispatch(const char *source);

} // namespace LunaDash::StallTrace
