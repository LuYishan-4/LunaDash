#pragma once

#include <wayland-server-core.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

class ControlServer final {
public:
  using RequestHandler = std::function<std::string(std::string_view)>;
  // The answer to a subscribe request: its reply line, and the events the
  // connection then receives from broadcast(). No events: the reply is sent
  // and the connection closes, as for any other request.
  struct Subscription {
    std::string reply;
    std::vector<std::string> events;
  };
  // Asked first for every request; nothing when it is not a subscribe request.
  using SubscribeHandler =
      std::function<std::optional<Subscription>(std::string_view)>;

  ControlServer(wl_event_loop *eventLoop, std::string socketPath,
                RequestHandler handler, SubscribeHandler subscribe = {});
  ~ControlServer();

  ControlServer(const ControlServer &) = delete;
  ControlServer &operator=(const ControlServer &) = delete;

  // Sends `line` (without its newline) to every subscriber of `event`. A
  // subscriber that stops reading and falls far behind is disconnected.
  void broadcast(std::string_view event, const std::string &line);
  bool subscribed(std::string_view event) const;

private:
  struct Client;

  static int acceptConnections(int fd, uint32_t mask, void *data);

  void acceptPendingConnections();
  void serveClient(Client *client, uint32_t mask);
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
