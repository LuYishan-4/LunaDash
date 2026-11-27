#include "core/process/LoopTask.hpp"

#include <cstdint>
#include <utility>

#include <sys/eventfd.h>
#include <unistd.h>
#include <wayland-server-core.h>

namespace LunaDash {

LoopTask::LoopTask(wl_event_loop *loop, std::function<void()> work,
                   std::function<void()> done)
    : done_(std::move(done)) {
  fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (fd_ >= 0 && loop)
    source_ = wl_event_loop_add_fd(loop, fd_, WL_EVENT_READABLE,
                                   &LoopTask::onDone, this);
  if (!source_) {
    release();
    return;
  }
  thread_ = std::thread([this, work = std::move(work)] {
    work();
    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = write(fd_, &one, sizeof one);
  });
}

LoopTask::~LoopTask() { release(); }

void LoopTask::release() {
  if (thread_.joinable())
    thread_.join();
  if (source_)
    wl_event_source_remove(source_);
  source_ = nullptr;
  if (fd_ >= 0)
    close(fd_);
  fd_ = -1;
}

int LoopTask::onDone(int, unsigned, void *data) {
  auto *task = static_cast<LoopTask *>(data);
  task->release();
  const auto done = std::move(task->done_);
  if (done)
    done();
  return 0;
}

} // namespace LunaDash
