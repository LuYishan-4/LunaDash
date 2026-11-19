#pragma once

#include <functional>
#include <map>
#include <memory>

struct wlr_foreign_toplevel_manager_v1;

namespace LunaDash {

struct ClientWindow;


class ForeignToplevels final {
public:
  struct Requests {
    std::function<void(int window)> activate;
    std::function<void(int window)> close;
    std::function<void(int window, bool)> minimize;
    std::function<void(int window, bool)> maximize;
    std::function<void(int window, bool)> fullscreen;
  };

  ForeignToplevels(wlr_foreign_toplevel_manager_v1 *manager, Requests requests);
  ~ForeignToplevels();
  ForeignToplevels(const ForeignToplevels &) = delete;
  ForeignToplevels &operator=(const ForeignToplevels &) = delete;

  // Publishes `client` (mapped, not a helper window) or withdraws it. Only
  // changed fields reach clients.
  void update(const ClientWindow &client, bool focused);
  void remove(int window);

  struct Entry;

private:
  wlr_foreign_toplevel_manager_v1 *manager_ = nullptr;
  Requests requests_;
  std::map<int, std::unique_ptr<Entry>> entries_;
};

} // namespace LunaDash
