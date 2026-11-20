#include "compositor/input/ClipboardComponent.hpp"
#include "core/log/Log.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <signal.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {

constexpr std::size_t kMaximumMimeTypes = 128;
constexpr std::size_t kMaximumSelectionBytes = 64 * 1024 * 1024;
constexpr int kCaptureTimeoutMs = 10000;

struct OwnedDataSource {
  wlr_data_source base{};
  void *owner = nullptr;
  std::unordered_map<std::string, std::shared_ptr<const std::vector<char>>>
      mimeTypes;
};

void closeFd(int &fd) {
  if (fd < 0)
    return;
  ::close(fd);
  fd = -1;
}

ssize_t writeWithoutSigpipe(int fd, const void *data, std::size_t size) {
  sigset_t signalSet;
  sigset_t previousMask;
  sigset_t pending;
  sigemptyset(&signalSet);
  sigaddset(&signalSet, SIGPIPE);
  if (::sigprocmask(SIG_BLOCK, &signalSet, &previousMask) < 0)
    return ::write(fd, data, size);
  const bool alreadyPending =
      ::sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE);
  const auto result = ::write(fd, data, size);
  const int error = errno;
  if (result < 0 && error == EPIPE && !alreadyPending) {
    timespec timeout{};
    while (::sigtimedwait(&signalSet, nullptr, &timeout) < 0 &&
           errno == EINTR) {
    }
  }
  ::sigprocmask(SIG_SETMASK, &previousMask, nullptr);
  errno = error;
  return result;
}

} // namespace

struct WaylandCompositor::Impl::ClipboardComponent::Capture {
  wl_listener sourceDestroy{};
  ClipboardComponent *owner = nullptr;
  wlr_data_source *source = nullptr;
  wl_event_source *timeout = nullptr;
  std::vector<std::unique_ptr<ReadTransfer>> transfers;
  std::unordered_map<std::string, std::vector<char>> payloads;
  std::size_t pending = 0;
  std::size_t totalBytes = 0;
};

struct WaylandCompositor::Impl::ClipboardComponent::ReadTransfer {
  ClipboardComponent *owner = nullptr;
  Capture *capture = nullptr;
  wl_event_source *source = nullptr;
  int fd = -1;
  std::string mimeType;
  std::vector<char> bytes;
};

struct WaylandCompositor::Impl::ClipboardComponent::WriteTransfer {
  ClipboardComponent *owner = nullptr;
  wl_event_source *source = nullptr;
  int fd = -1;
  std::shared_ptr<const std::vector<char>> bytes;
  std::size_t offset = 0;
};

WaylandCompositor::Impl::ClipboardComponent::ClipboardComponent(Impl *owner)
    : Component(owner) {}

void WaylandCompositor::Impl::ClipboardComponent::sourceSend(
    wlr_data_source *source, const char *mimeType, int fd) {
  auto *owned = reinterpret_cast<OwnedDataSource *>(source);
  if (!owned->owner || !mimeType) {
    closeFd(fd);
    return;
  }
  static_cast<WaylandCompositor::Impl::ClipboardComponent *>(owned->owner)
      ->sendStoredSelection(mimeType, fd);
}

void WaylandCompositor::Impl::ClipboardComponent::sourceDestroy(
    wlr_data_source *source) {
  auto *owned = reinterpret_cast<OwnedDataSource *>(source);
  if (owned->owner)
    static_cast<WaylandCompositor::Impl::ClipboardComponent *>(owned->owner)
        ->onSourceDestroy();
  delete owned;
}

const wlr_data_source_impl *
WaylandCompositor::Impl::ClipboardComponent::sourceImplementation() {
  static const wlr_data_source_impl implementation = {
      .send = sourceSend,
      .accept = nullptr,
      .destroy = sourceDestroy,
      .dnd_drop = nullptr,
      .dnd_finish = nullptr,
      .dnd_action = nullptr,
  };
  return &implementation;
}

WaylandCompositor::Impl::ClipboardComponent::~ClipboardComponent() {
  cancelCapture();
  for (const auto &write : writes_) {
    if (write->source)
      wl_event_source_remove(write->source);
    closeFd(write->fd);
  }
  writes_.clear();

  if (!ownedSource_)
    return;
  auto *owned = reinterpret_cast<OwnedDataSource *>(ownedSource_);
  owned->owner = nullptr;
  auto *source = std::exchange(ownedSource_, nullptr);
  if (impl->seat && impl->seat->selection_source == source)
    wlr_seat_set_selection(impl->seat, nullptr,
                           wl_display_next_serial(impl->display));
  else
    wlr_data_source_destroy(source);
}

bool WaylandCompositor::Impl::ClipboardComponent::start() {
  wlr_data_device_manager_create(impl->display);
#if LUDASH_WLR_HAS_DATA_CONTROL

  impl->dataControl = wlr_data_control_manager_v1_create(impl->display);
  if (!impl->dataControl)
    return impl->fail("wlroots could not create the data-control manager.");
#endif
  listen<&ClipboardComponent::onRequestSelection>(
      &impl->seat->events.request_set_selection, requestSelection);
  listen<&ClipboardComponent::onSelection>(&impl->seat->events.set_selection,
                                           selection);
  return true;
}

void WaylandCompositor::Impl::ClipboardComponent::onRequestSelection(
    wlr_seat_request_set_selection_event *event) {
  if (event)
    wlr_seat_set_selection(impl->seat, event->source, event->serial);
}

void WaylandCompositor::Impl::ClipboardComponent::onSelection() {
  auto *source = impl->seat->selection_source;
  if (source && source == ownedSource_)
    return;

  cancelCapture();
  if (!source)
    return;

  const auto mimeCount = source->mime_types.size / sizeof(char *);
  if (mimeCount == 0 || mimeCount > kMaximumMimeTypes) {
    logWarning("Could not preserve clipboard selection with %zu MIME types.",
               mimeCount);
    return;
  }

  auto capture = std::make_unique<Capture>();
  capture->owner = this;
  capture->source = source;
  capture->sourceDestroy.notify = [](wl_listener *listener, void *) {
    auto *capture = reinterpret_cast<Capture *>(listener);
    capture->owner->onSourceDestroy();
  };
  wl_signal_add(&source->events.destroy, &capture->sourceDestroy);
  capture_ = std::move(capture);

  capture_->timeout = wl_event_loop_add_timer(
      impl->eventLoop,
      [](void *data) {
        static_cast<ClipboardComponent *>(data)->onCaptureTimeout();
        return 0;
      },
      this);
  if (!capture_->timeout ||
      wl_event_source_timer_update(capture_->timeout, kCaptureTimeoutMs) < 0) {
    logWarning("Could not schedule clipboard preservation.");
    cancelCapture();
    return;
  }

  auto **mimeTypes = static_cast<char **>(source->mime_types.data);
  std::unordered_set<std::string> seen;
  for (std::size_t index = 0; index < mimeCount; ++index) {
    if (!mimeTypes[index] || !*mimeTypes[index])
      continue;
    std::string mimeType(mimeTypes[index]);
    if (!seen.insert(mimeType).second)
      continue;

    int pipeFds[2];
    if (::pipe(pipeFds) < 0) {
      logWarning("Could not create a clipboard transfer pipe: %s.",
                 std::strerror(errno));
      cancelCapture();
      return;
    }
    const int flags = ::fcntl(pipeFds[0], F_GETFL);
    if (flags < 0 || ::fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
      closeFd(pipeFds[0]);
      closeFd(pipeFds[1]);
      logWarning("Could not configure a clipboard transfer pipe: %s.",
                 std::strerror(errno));
      cancelCapture();
      return;
    }

    auto transfer = std::make_unique<ReadTransfer>();
    transfer->owner = this;
    transfer->capture = capture_.get();
    transfer->fd = pipeFds[0];
    transfer->mimeType = std::move(mimeType);
    transfer->source = wl_event_loop_add_fd(
        impl->eventLoop, transfer->fd, WL_EVENT_READABLE | WL_EVENT_HANGUP,
        [](int, uint32_t mask, void *data) {
          auto *transfer = static_cast<ReadTransfer *>(data);
          transfer->owner->onCaptureReadable(transfer, mask);
          return 0;
        },
        transfer.get());
    if (!transfer->source) {
      closeFd(transfer->fd);
      closeFd(pipeFds[1]);
      logWarning("Could not monitor clipboard data.");
      cancelCapture();
      return;
    }

    auto *pending = transfer.get();
    capture_->transfers.push_back(std::move(transfer));
    ++capture_->pending;
    wlr_data_source_send(source, pending->mimeType.c_str(), pipeFds[1]);
  }

  if (capture_ && capture_->pending == 0) {
    logWarning("Clipboard selection offered no usable MIME types.");
    cancelCapture();
  }
}

void WaylandCompositor::Impl::ClipboardComponent::onSourceDestroy() {
  ownedSource_ = nullptr;
  cancelCapture();
}

void WaylandCompositor::Impl::ClipboardComponent::onCaptureTimeout() {
  if (!capture_)
    return;
  logWarning("Timed out while preserving clipboard data.");
  cancelCapture();
}

void WaylandCompositor::Impl::ClipboardComponent::onCaptureReadable(
    ReadTransfer *transfer, uint32_t) {
  if (!capture_ || transfer->capture != capture_.get())
    return;

  char buffer[8192];
  while (true) {
    const auto count = ::read(transfer->fd, buffer, sizeof(buffer));
    if (count > 0) {
      const auto bytes = static_cast<std::size_t>(count);
      if (bytes > kMaximumSelectionBytes - capture_->totalBytes) {
        logWarning(
            "Clipboard selection exceeded the 64 MiB preservation limit.");
        cancelCapture();
        return;
      }
      transfer->bytes.insert(transfer->bytes.end(), buffer, buffer + count);
      capture_->totalBytes += bytes;
      continue;
    }
    if (count == 0) {
      auto *capture = capture_.get();
      capture->payloads.emplace(transfer->mimeType, std::move(transfer->bytes));
      if (transfer->source)
        wl_event_source_remove(transfer->source);
      closeFd(transfer->fd);
      const auto found =
          std::find_if(capture->transfers.begin(), capture->transfers.end(),
                       [transfer](const auto &pending) {
                         return pending.get() == transfer;
                       });
      if (found != capture->transfers.end())
        capture->transfers.erase(found);
      --capture->pending;
      if (capture->pending == 0)
        finishCapture();
      return;
    }
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;
    logWarning("Could not read clipboard data: %s.", std::strerror(errno));
    cancelCapture();
    return;
  }
}

void WaylandCompositor::Impl::ClipboardComponent::finishCapture() {
  if (!capture_)
    return;
  auto capture = std::move(capture_);
  if (capture->timeout)
    wl_event_source_remove(capture->timeout);
  wl_list_remove(&capture->sourceDestroy.link);

  if (impl->seat->selection_source != capture->source ||
      capture->payloads.empty())
    return;

  auto *owned = new OwnedDataSource;
  owned->owner = this;
  wlr_data_source_init(&owned->base, sourceImplementation());
  for (auto &[mimeType, bytes] : capture->payloads) {
    auto *name = static_cast<char *>(std::malloc(mimeType.size() + 1));
    if (!name) {
      wlr_data_source_destroy(&owned->base);
      logWarning("Could not preserve clipboard MIME types.");
      return;
    }
    std::memcpy(name, mimeType.c_str(), mimeType.size() + 1);
    auto *slot = static_cast<char **>(
        wl_array_add(&owned->base.mime_types, sizeof(char *)));
    if (!slot) {
      std::free(name);
      wlr_data_source_destroy(&owned->base);
      logWarning("Could not preserve clipboard MIME types.");
      return;
    }
    *slot = name;
    owned->mimeTypes.emplace(
        mimeType, std::make_shared<const std::vector<char>>(std::move(bytes)));
  }
  ownedSource_ = &owned->base;
  wlr_seat_set_selection(impl->seat, ownedSource_,
                         wl_display_next_serial(impl->display));
}

void WaylandCompositor::Impl::ClipboardComponent::cancelCapture() {
  if (!capture_)
    return;
  auto capture = std::move(capture_);
  if (capture->timeout)
    wl_event_source_remove(capture->timeout);
  if (capture->sourceDestroy.link.next && capture->sourceDestroy.link.prev)
    wl_list_remove(&capture->sourceDestroy.link);
  for (const auto &transfer : capture->transfers) {
    if (transfer->source)
      wl_event_source_remove(transfer->source);
    closeFd(transfer->fd);
  }
}

void WaylandCompositor::Impl::ClipboardComponent::sendStoredSelection(
    const char *mimeType, int fd) {
  if (!ownedSource_ || !mimeType) {
    closeFd(fd);
    return;
  }
  auto *owned = reinterpret_cast<OwnedDataSource *>(ownedSource_);
  const auto found = owned->mimeTypes.find(mimeType);
  if (found == owned->mimeTypes.end()) {
    closeFd(fd);
    return;
  }

  const int flags = ::fcntl(fd, F_GETFL);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    closeFd(fd);
    return;
  }
  auto transfer = std::make_unique<WriteTransfer>();
  transfer->owner = this;
  transfer->fd = fd;
  transfer->bytes = found->second;
  transfer->source = wl_event_loop_add_fd(
      impl->eventLoop, fd, WL_EVENT_WRITABLE | WL_EVENT_HANGUP,
      [](int, uint32_t mask, void *data) {
        auto *transfer = static_cast<WriteTransfer *>(data);
        transfer->owner->onWriteReady(transfer, mask);
        return 0;
      },
      transfer.get());
  if (!transfer->source) {
    closeFd(transfer->fd);
    return;
  }
  writes_.push_back(std::move(transfer));
}

void WaylandCompositor::Impl::ClipboardComponent::onWriteReady(
    WriteTransfer *transfer, uint32_t) {
  while (transfer->offset < transfer->bytes->size()) {
    const auto count = writeWithoutSigpipe(
        transfer->fd, transfer->bytes->data() + transfer->offset,
        transfer->bytes->size() - transfer->offset);
    if (count > 0) {
      transfer->offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return;
    finishWrite(transfer);
    return;
  }
  finishWrite(transfer);
}

void WaylandCompositor::Impl::ClipboardComponent::finishWrite(
    WriteTransfer *transfer) {
  if (transfer->source)
    wl_event_source_remove(transfer->source);
  closeFd(transfer->fd);
  const auto found = std::find_if(
      writes_.begin(), writes_.end(),
      [transfer](const auto &pending) { return pending.get() == transfer; });
  if (found != writes_.end())
    writes_.erase(found);
}

} // namespace LunaDash
