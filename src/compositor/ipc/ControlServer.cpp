#include "compositor/ipc/ControlServer.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace LunaDash {
namespace {
constexpr std::size_t kMaximumRequestBytes = 65536;
constexpr int kClientTimeoutMs = 3000;

[[noreturn]] void throwSystemError(const char *operation) {
  throw std::system_error(errno, std::generic_category(), operation);
}

bool isStaleOwnedSocket(const std::string &path) {
  struct stat status{};
  if (::lstat(path.c_str(), &status) != 0)
    return errno == ENOENT;
  return S_ISSOCK(status.st_mode) && status.st_uid == ::geteuid();
}

bool removeStaleSocket(const std::string &path) {
  if (!isStaleOwnedSocket(path))
    return false;

  const int probe = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (probe < 0)
    return false;

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
  const int result =
      ::connect(probe, reinterpret_cast<sockaddr *>(&address), sizeof(address));
  const int connectError = errno;
  ::close(probe);
  if (result == 0 || (connectError != ECONNREFUSED && connectError != ENOENT))
    return false;

  struct stat current{};
  if (::lstat(path.c_str(), &current) != 0)
    return errno == ENOENT;
  if (!S_ISSOCK(current.st_mode) || current.st_uid != ::geteuid())
    return false;
  return ::unlink(path.c_str()) == 0 || errno == ENOENT;
}

void removeOwnedSocket(const std::string &path, unsigned long long device,
                       unsigned long long inode) {
  struct stat current{};
  if (::lstat(path.c_str(), &current) == 0 && S_ISSOCK(current.st_mode) &&
      current.st_uid == ::geteuid() &&
      static_cast<unsigned long long>(current.st_dev) == device &&
      static_cast<unsigned long long>(current.st_ino) == inode)
    ::unlink(path.c_str());
}
} // namespace

struct ControlServer::Client {
  ControlServer *owner = nullptr;
  int fd = -1;
  wl_event_source *socketSource = nullptr;
  wl_event_source *timeoutSource = nullptr;
  std::string request;
  std::string response;
  std::size_t responseOffset = 0;
};

ControlServer::ControlServer(wl_event_loop *eventLoop, std::string socketPath,
                             RequestHandler handler)
    : eventLoop_(eventLoop), socketPath_(std::move(socketPath)),
      handler_(std::move(handler)) {
  if (!eventLoop_ || !handler_ || socketPath_.empty())
    throw std::invalid_argument(
        "ControlServer requires an event loop, path, and handler");

  sockaddr_un address{};
  if (socketPath_.size() >= sizeof(address.sun_path))
    throw std::length_error("LunaDash control socket path is too long");
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, socketPath_.c_str(), socketPath_.size() + 1);

  listenerFd_ =
      ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listenerFd_ < 0)
    throwSystemError("create LunaDash control socket");

  if (::bind(listenerFd_, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
    int error = errno;
    if (error == EADDRINUSE && removeStaleSocket(socketPath_)) {
      if (::bind(listenerFd_, reinterpret_cast<sockaddr *>(&address),
                 sizeof(address)) == 0)
        error = 0;
      else
        error = errno;
    }
    if (error != 0) {
      ::close(listenerFd_);
      listenerFd_ = -1;
      throw std::system_error(error, std::generic_category(),
                              "bind LunaDash control socket");
    }
  }

  try {
    updateSocketOwnership();
  } catch (...) {
    ::close(listenerFd_);
    listenerFd_ = -1;
    throw;
  }
  if (::chmod(socketPath_.c_str(), S_IRUSR | S_IWUSR) != 0 ||
      ::listen(listenerFd_, 16) != 0) {
    const int error = errno;
    ::close(listenerFd_);
    listenerFd_ = -1;
    removeOwnedSocket(socketPath_, socketDevice_, socketInode_);
    throw std::system_error(error, std::generic_category(),
                            "prepare LunaDash control socket");
  }

  listenerSource_ = wl_event_loop_add_fd(
      eventLoop_, listenerFd_, WL_EVENT_READABLE, acceptConnections, this);
  if (!listenerSource_) {
    const int error = errno;
    ::close(listenerFd_);
    listenerFd_ = -1;
    removeOwnedSocket(socketPath_, socketDevice_, socketInode_);
    throw std::system_error(error == 0 ? ENOMEM : error,
                            std::generic_category(),
                            "register LunaDash control socket");
  }
}

ControlServer::~ControlServer() {
  while (!clients_.empty())
    closeClient(clients_.begin()->second.get());
  if (listenerSource_)
    wl_event_source_remove(listenerSource_);
  if (listenerFd_ >= 0)
    ::close(listenerFd_);

  removeOwnedSocket(socketPath_, socketDevice_, socketInode_);
}

int ControlServer::acceptConnections(int, uint32_t mask, void *data) {
  auto *server = static_cast<ControlServer *>(data);
  if (!server || (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) != 0)
    return 0;
  server->acceptPendingConnections();
  return 0;
}

int ControlServer::handleClient(int, uint32_t mask, void *data) {
  auto *client = static_cast<Client *>(data);
  if (!client || !client->owner)
    return 0;
  if ((mask & WL_EVENT_ERROR) != 0 ||
      ((mask & WL_EVENT_HANGUP) != 0 && (mask & WL_EVENT_READABLE) == 0)) {
    client->owner->closeClient(client);
    return 0;
  }

  if ((mask & WL_EVENT_READABLE) != 0 && client->response.empty()) {
    char bytes[4096];
    for (;;) {
      const ssize_t count = ::recv(client->fd, bytes, sizeof(bytes), 0);
      if (count > 0) {
        client->request.append(bytes, static_cast<std::size_t>(count));
        if (client->request.size() > kMaximumRequestBytes) {
          client->owner->closeClient(client);
          return 0;
        }
        const auto newline = client->request.find('\n');
        if (newline != std::string::npos) {
          try {
            client->response = client->owner->handler_(
                std::string_view(client->request.data(), newline));
          } catch (const std::exception &) {
            client->response = "{\"error\":\"Control handler failed\"}";
          } catch (...) {
            client->response = "{\"error\":\"Control handler failed\"}";
          }
          client->response.push_back('\n');
          wl_event_source_fd_update(client->socketSource, WL_EVENT_WRITABLE);
          break;
        }
        if (static_cast<std::size_t>(count) < sizeof(bytes))
          break;
        continue;
      }
      if (count == 0) {
        client->owner->closeClient(client);
        return 0;
      }
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      client->owner->closeClient(client);
      return 0;
    }
  }

  if ((mask & WL_EVENT_HANGUP) != 0 && client->response.empty()) {
    client->owner->closeClient(client);
    return 0;
  }

  if ((mask & WL_EVENT_WRITABLE) != 0 && !client->response.empty()) {
    while (client->responseOffset < client->response.size()) {
      const ssize_t count = ::send(
          client->fd, client->response.data() + client->responseOffset,
          client->response.size() - client->responseOffset, MSG_NOSIGNAL);
      if (count > 0) {
        client->responseOffset += static_cast<std::size_t>(count);
        continue;
      }
      if (count < 0 && errno == EINTR)
        continue;
      if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 0;
      client->owner->closeClient(client);
      return 0;
    }
    client->owner->closeClient(client);
  }
  return 0;
}

int ControlServer::expireClient(void *data) {
  auto *client = static_cast<Client *>(data);
  if (client && client->owner)
    client->owner->closeClient(client);
  return 0;
}

void ControlServer::acceptPendingConnections() {
  for (;;) {
    const int fd =
        ::accept4(listenerFd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      return;
    }

    auto client = std::make_unique<Client>();
    client->owner = this;
    client->fd = fd;
    client->socketSource = wl_event_loop_add_fd(
        eventLoop_, fd, WL_EVENT_READABLE, handleClient, client.get());
    client->timeoutSource =
        wl_event_loop_add_timer(eventLoop_, expireClient, client.get());
    if (!client->socketSource || !client->timeoutSource) {
      if (client->socketSource)
        wl_event_source_remove(client->socketSource);
      if (client->timeoutSource)
        wl_event_source_remove(client->timeoutSource);
      ::close(fd);
      continue;
    }
    wl_event_source_timer_update(client->timeoutSource, kClientTimeoutMs);
    clients_.emplace(fd, std::move(client));
  }
}

void ControlServer::closeClient(Client *client) {
  if (!client)
    return;
  const int fd = client->fd;
  const auto found = clients_.find(fd);
  if (found == clients_.end()) {
    if (client->socketSource)
      wl_event_source_remove(client->socketSource);
    if (client->timeoutSource)
      wl_event_source_remove(client->timeoutSource);
    if (fd >= 0)
      ::close(fd);
    client->fd = -1;
    client->owner = nullptr;
    return;
  }
  auto owned = std::move(found->second);
  clients_.erase(found);
  owned->owner = nullptr;
  if (owned->socketSource)
    wl_event_source_remove(owned->socketSource);
  if (owned->timeoutSource)
    wl_event_source_remove(owned->timeoutSource);
  if (owned->fd >= 0)
    ::close(owned->fd);
  owned->fd = -1;
}

void ControlServer::updateSocketOwnership() {
  struct stat status{};
  if (::lstat(socketPath_.c_str(), &status) != 0)
    throwSystemError("inspect LunaDash control socket");
  if (!S_ISSOCK(status.st_mode) || status.st_uid != ::geteuid())
    throw std::system_error(EPERM, std::generic_category(),
                            "verify LunaDash control socket ownership");
  socketDevice_ = static_cast<unsigned long long>(status.st_dev);
  socketInode_ = static_cast<unsigned long long>(status.st_ino);
}

} // namespace LunaDash
