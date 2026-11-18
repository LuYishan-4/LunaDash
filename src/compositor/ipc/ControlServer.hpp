#pragma once

#include <wayland-server-core.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {

class ControlServer final {
public:
  using RequestHandler = std::function<std::string(std::string_view)>;
  // Asked first for every request: a value makes the connection a subscriber
  // that stays open, starts with that line and then receives broadcast().
  using SubscribeHandler =
      std::function<std::optional<std::string>(std::string_view)>;

  ControlServer(wl_event_loop *eventLoop, std::string socketPath,
                RequestHandler handler, SubscribeHandler subscribe = {});
  ~ControlServer();

  ControlServer(const ControlServer &) = delete;
  ControlServer &operator=(const ControlServer &) = delete;

  // Sends `line` (without its newline) to every subscriber. A subscriber that
  // stops reading and falls far behind is disconnected.
  void broadcast(const std::string &line);
  std::size_t subscribers() const;

private:
  struct Client;

  static int acceptConnections(int fd, uint32_t mask, void *data);
  static int handleClient(int fd, uint32_t mask, void *data);
  static int expireClient(void *data);

  void acceptPendingConnections();
  void closeClient(Client *client);
  // Writes what a client has queued; false when it had to be closed.
  bool flushClient(Client *client);
  void updateSocketOwnership();

  wl_event_loop *eventLoop_ = nullptr;
  wl_event_source *listenerSource_ = nullptr;
  int listenerFd_ = -1;
  std::string socketPath_;
  RequestHandler handler_;
  SubscribeHandler subscribe_;
  std::map<int, std::unique_ptr<Client>> clients_;
  unsigned long long socketDevice_ = 0;
  unsigned long long socketInode_ = 0;
};

} // namespace LunaDash
