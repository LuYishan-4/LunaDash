#pragma once

#include "core/process/ProcessRunner.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace LunaDash {

class ScreenCapture final {
public:
  // grim captures outputs through this wlroots protocol.
  static constexpr const char *protocol = "zwlr_screencopy_manager_v1";
  static constexpr int protocolVersion = 3;

  using Completion =
      std::function<void(const std::string &path, const std::string &error)>;

  explicit ScreenCapture(wl_event_loop *eventLoop);
  ~ScreenCapture();

  ScreenCapture(const ScreenCapture &) = delete;
  ScreenCapture &operator=(const ScreenCapture &) = delete;

  void setCompletion(Completion completion);
  bool selectRegion(const ProcessEnvironment &environment,
                    const std::string &path, std::string *error);
  bool captureOutput(const ProcessEnvironment &environment,
                     const std::string &path, std::string *error);
  bool busy() const;
  constexpr std::string_view phase() const noexcept {
    if (phase_ & PhaseFailed)
      return "failed";
    if (phase_ & PhaseCancelled)
      return "cancelled";
    if (phase_ & PhaseSaved)
      return "saved";
    if (phase_ & PhaseCapturing)
      return "capturing";
    if (phase_ & PhaseSelecting)
      return "selecting";
    return "idle";
  }

private:
  enum PhaseFlags : unsigned {
    PhaseIdle = 0,
    PhaseSelecting = 1u << 0,
    PhaseCapturing = 1u << 1,
    PhaseSaved = 1u << 2,
    PhaseCancelled = 1u << 3,
    PhaseFailed = 1u << 4,
  };

  ProcessRunner process_;
  ProcessEnvironment environment_;
  Completion completion_;
  std::string destination_;
  std::string temporaryPath_;
  std::string grim_;
  unsigned phase_ = PhaseIdle;

  bool prepareDestination(const std::string &path, std::string *error);
  bool createTemporary(std::string *error);
  // By value: callers pass destination_, which finish() clears before the completion runs.
  void finish(std::string path, std::string error);
  void processFinished(const ProcessResult &result);
  bool saveTemporary(std::string *error);
  void removeTemporary();
};

} // namespace LunaDash
