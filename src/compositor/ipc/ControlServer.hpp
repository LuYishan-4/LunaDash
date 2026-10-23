#pragma once

#include <wayland-server-core.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace LunaDash {

class ControlServer final {
public:
  using RequestHandler = std::function<std::string(std::string_view)>;

  ControlServer(wl_event_loop *eventLoop, std::string socketPath,
                RequestHandler handler);
  ~ControlServer();

  ControlServer(const ControlServer &) = delete;
  ControlServer &operator=(const ControlServer &) = delete;

private:
  struct Client;

  static int acceptConnections(int fd, uint32_t mask, void *data);
  static int handleClient(int fd, uint32_t mask, void *data);
  static int expireClient(void *data);

  void acceptPendingConnections();
  void closeClient(Client *client);
  void updateSocketOwnership();

  wl_event_loop *eventLoop_ = nullptr;
  wl_event_source *listenerSource_ = nullptr;
  int listenerFd_ = -1;
  std::string socketPath_;
  RequestHandler handler_;
  std::map<int, std::unique_ptr<Client>> clients_;
  unsigned long long socketDevice_ = 0;
  unsigned long long socketInode_ = 0;
};

} // namespace LunaDash
