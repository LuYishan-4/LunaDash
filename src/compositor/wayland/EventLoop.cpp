#include "compositor/wayland/Register.hpp"
#include "core/log/Log.hpp"
#include <algorithm>
#include <cerrno>
#include <glib.h>
#include <poll.h>
#include <vector>

namespace LunaDash {

bool WaylandCompositor::Impl::fail(const char *message) {
  logCritical("LunaDash wlroots: %s", message);
  q->processFailure_ = true;
  terminateEventLoop(2);
  return false;
}

int WaylandCompositor::Impl::runEventLoop(bool profile) {
  if (!display || !eventLoop)
    return 2;
  const int eventLoopFd = wl_event_loop_get_fd(eventLoop);
  if (eventLoopFd < 0)
    return 2;
  // Desktop services use GIO (file monitors, D-Bus and async I/O). Drive the
  // default GLib context from the same poll so those sources run on the
  // compositor thread without a second event loop or toolkit dispatcher.
  GMainContext *context = g_main_context_default();
  const bool ownsContext = g_main_context_acquire(context);
  std::vector<GPollFD> glibDescriptors(16);
  std::vector<pollfd> descriptors;
  while (eventLoopRunning) {
    const auto started = std::chrono::steady_clock::now();
    if (wl_event_loop_dispatch(eventLoop, 0) < 0) {
      logCritical("LunaDash wlroots event dispatch failed.");
      q->processFailure_ = true;
      exitCode = 2;
      break;
    }
    wl_display_flush_clients(display);
    ++dispatchCalls;
    lastDispatchUsec =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
    maxDispatchUsec = std::max(maxDispatchUsec, lastDispatchUsec);
    if (lastDispatchUsec >= 8000)
      ++slowDispatches;
    if (profile && lastDispatchUsec > 25000)
      logWarning("LunaDash event-loop dispatch: %.3f ms",
                 static_cast<double>(lastDispatchUsec) / 1000.0);
    if (!eventLoopRunning)
      break;

    gint maxPriority = 0;
    gint timeout = -1;
    gint glibCount = 0;
    if (ownsContext) {
      g_main_context_prepare(context, &maxPriority);
      while ((glibCount = g_main_context_query(
                  context, maxPriority, &timeout, glibDescriptors.data(),
                  static_cast<gint>(glibDescriptors.size()))) >
             static_cast<gint>(glibDescriptors.size()))
        glibDescriptors.resize(static_cast<std::size_t>(glibCount));
    }
    descriptors.clear();
    descriptors.push_back({eventLoopFd, POLLIN, 0});
    for (gint index = 0; index < glibCount; ++index)
      descriptors.push_back({glibDescriptors[static_cast<std::size_t>(index)].fd,
                             static_cast<short>(
                                 glibDescriptors[static_cast<std::size_t>(index)]
                                     .events),
                             0});
    int result;
    do {
      result = ::poll(descriptors.data(), descriptors.size(), timeout);
    } while (result < 0 && errno == EINTR);
    if (result < 0 ||
        (descriptors.front().revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      logCritical("LunaDash could not poll the wlroots event loop.");
      q->processFailure_ = true;
      exitCode = 2;
      break;
    }
    if (ownsContext) {
      for (gint index = 0; index < glibCount; ++index)
        glibDescriptors[static_cast<std::size_t>(index)].revents =
            static_cast<gushort>(
                descriptors[static_cast<std::size_t>(index) + 1].revents);
      if (g_main_context_check(context, maxPriority, glibDescriptors.data(),
                               glibCount))
        g_main_context_dispatch(context);
    }
  }
  if (ownsContext)
    g_main_context_release(context);
  return exitCode;
}

void WaylandCompositor::Impl::terminateEventLoop(int status) {
  exitCode = status;
  eventLoopRunning = false;
}

bool WaylandCompositor::Impl::scheduleScratchpadTimeout(std::uint64_t generation) {
  cancelScratchpadTimeout();
  if (!eventLoop)
    return false;
  scratchpadTimeout =
      Templates::addTimer<&Impl::onScratchpadTimeout>(eventLoop, this);
  if (!scratchpadTimeout) {
    logCritical("Could not create the scratchpad launch timer.");
    return false;
  }
  scratchpadTimeoutGeneration = generation;
  if (wl_event_source_timer_update(scratchpadTimeout, 15000) < 0) {
    cancelScratchpadTimeout();
    logCritical("Could not schedule the scratchpad launch timer.");
    return false;
  }
  return true;
}

void WaylandCompositor::Impl::cancelScratchpadTimeout() {
  if (!scratchpadTimeout)
    return;
  wl_event_source_remove(scratchpadTimeout);
  scratchpadTimeout = nullptr;
}

void WaylandCompositor::Impl::onScratchpadTimeout() {
  cancelScratchpadTimeout();
  if (q && q->scratchpadPending_ &&
      q->scratchpadGeneration_ == scratchpadTimeoutGeneration) {
    q->scratchpadPending_ = false;
    q->scratchpadProcess_ = 0;
    q->scratchpadError_ =
        "The terminal did not create a new window. Configure "
        "the terminal role to start a separate process.";
  }
}

bool WaylandCompositor::Impl::scheduleCallback(
    int delayMilliseconds, std::function<void()> callback) {
  if (!eventLoop || !callback)
    return false;
  auto scheduled = std::make_unique<ScheduledCallback>();
  scheduled->impl = this;
  scheduled->callback = std::move(callback);
  auto *entry = scheduled.get();
  scheduledCallbacks.push_back(std::move(scheduled));
  entry->source =
      Templates::addTimer<&ScheduledCallback::onTimer>(eventLoop, entry);
  if (!entry->source ||
      // A zero delay disarms a wl_event_loop timer; dispatch "now" as 1 ms.
      wl_event_source_timer_update(entry->source,
                                   std::max(1, delayMilliseconds)) < 0) {
    if (entry->source)
      wl_event_source_remove(entry->source);
    const auto found = std::find_if(
        scheduledCallbacks.begin(), scheduledCallbacks.end(),
        [entry](const auto &pending) { return pending.get() == entry; });
    if (found != scheduledCallbacks.end())
      scheduledCallbacks.erase(found);
    logCritical("Could not schedule a compositor event-loop callback.");
    return false;
  }
  return true;
}

void WaylandCompositor::Impl::cancelScheduledCallbacks() {
  for (const auto &entry : scheduledCallbacks)
    if (entry && entry->source)
      wl_event_source_remove(entry->source);
  scheduledCallbacks.clear();
}

void WaylandCompositor::Impl::ScheduledCallback::onTimer() {
  auto run = std::move(callback);
  auto &pending = impl->scheduledCallbacks;
  const auto self =
      std::find_if(pending.begin(), pending.end(),
                   [this](const auto &entry) { return entry.get() == this; });
  if (self != pending.end()) {
    if (source)
      wl_event_source_remove(source);
    pending.erase(self); // `this` is gone from here on.
  }
  if (run)
    run();
}

} // namespace LunaDash
