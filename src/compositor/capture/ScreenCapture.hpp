#pragma once

#include "core/process/ProcessRunner.hpp"

#include <functional>
#include <string>

namespace LunaDash {

class ScreenCapture final {
public:
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
  const std::string &phase() const;

private:
  ProcessRunner process_;
  ProcessEnvironment environment_;
  Completion completion_;
  std::string destination_;
  std::string temporaryPath_;
  std::string grim_;
  std::string phase_ = "idle";

  bool prepareDestination(const std::string &path, std::string *error);
  bool createTemporary(std::string *error);
  void finish(const std::string &path, const std::string &error);
  void processFinished(const ProcessResult &result);
  bool saveTemporary(std::string *error);
  void removeTemporary();
};

} // namespace LunaDash
