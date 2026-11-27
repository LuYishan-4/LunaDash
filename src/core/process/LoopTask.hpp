#pragma once

#include <functional>
#include <thread>

struct wl_event_loop;
struct wl_event_source;

namespace LunaDash {

// Runs `work` on a thread of its own, then `done` on `loop` once it returns.
// `done` may destroy the task. Destroying it earlier waits for `work`, and
// `done` never runs.
class LoopTask {
public:
  LoopTask(wl_event_loop *loop, std::function<void()> work,
           std::function<void()> done);
  ~LoopTask();
  LoopTask(const LoopTask &) = delete;
  LoopTask &operator=(const LoopTask &) = delete;

  // False when the thread or its wake-up could not be set up.
  bool started() const { return thread_.joinable(); }

private:
  static int onDone(int fd, unsigned mask, void *data);
  void release();

  std::function<void()> done_;
  std::thread thread_;
  int fd_ = -1;
  wl_event_source *source_ = nullptr;
};

} // namespace LunaDash
