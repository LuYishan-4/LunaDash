#include "compositor/wayland/ForeignToplevels.hpp"

// C++ library headers must be parsed before the wlroots compatibility macros.
#include <cstdlib>
#include <string>

#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/WaylandHook.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT

struct ForeignToplevels::Entry {
  wlr_foreign_toplevel_handle_v1 *handle = nullptr;
  std::string title, appId;
  bool activated = false, minimized = false, maximized = false,
       fullscreen = false, announced = false;
  WaylandHook onActivate, onClose, onMinimize, onMaximize, onFullscreen;

  ~Entry() {
    for (auto *hook : {&onActivate, &onClose, &onMinimize, &onMaximize, &onFullscreen})
      hook->detach();
    if (handle)
      wlr_foreign_toplevel_handle_v1_destroy(handle);
  }
};

ForeignToplevels::ForeignToplevels(wlr_foreign_toplevel_manager_v1 *manager,
                                   Requests requests)
    : manager_(manager), requests_(std::move(requests)) {}

ForeignToplevels::~ForeignToplevels() = default;

void ForeignToplevels::update(const ClientWindow &client, bool focused) {
  if (!manager_)
    return;
  if (!client.mapped || client.utility) {
    remove(client.id);
    return;
  }
  auto &slot = entries_[client.id];
  if (!slot) {
    auto entry = std::make_unique<Entry>();
    entry->handle = wlr_foreign_toplevel_handle_v1_create(manager_);
    if (!entry->handle) {
      entries_.erase(client.id);
      return;
    }
    const int id = client.id;
    auto &requests = requests_;
    entry->onActivate.attach(&entry->handle->events.request_activate,
                           [&requests, id](void *) {
                             if (requests.activate)
                               requests.activate(id);
                           });
    entry->onClose.attach(&entry->handle->events.request_close,
                        [&requests, id](void *) {
                          if (requests.close)
                            requests.close(id);
                        });
    entry->onMinimize.attach(&entry->handle->events.request_minimize,
                           [&requests, id](void *data) {
                             auto *event = static_cast<
                                 wlr_foreign_toplevel_handle_v1_minimized_event *>(data);
                             if (requests.minimize && event)
                               requests.minimize(id, event->minimized);
                           });
    entry->onMaximize.attach(&entry->handle->events.request_maximize,
                           [&requests, id](void *data) {
                             auto *event = static_cast<
                                 wlr_foreign_toplevel_handle_v1_maximized_event *>(data);
                             if (requests.maximize && event)
                               requests.maximize(id, event->maximized);
                           });
    entry->onFullscreen.attach(&entry->handle->events.request_fullscreen,
                             [&requests, id](void *data) {
                               auto *event = static_cast<
                                   wlr_foreign_toplevel_handle_v1_fullscreen_event *>(data);
                               if (requests.fullscreen && event)
                                 requests.fullscreen(id, event->fullscreen);
                             });
    slot = std::move(entry);
  }
  auto &entry = *slot;
  auto *handle = entry.handle;
  const bool activated = focused && !client.minimized;
  if (!entry.announced || entry.title != client.title) {
    entry.title = client.title;
    wlr_foreign_toplevel_handle_v1_set_title(handle, entry.title.c_str());
  }
  if (!entry.announced || entry.appId != client.appId) {
    entry.appId = client.appId;
    wlr_foreign_toplevel_handle_v1_set_app_id(handle, entry.appId.c_str());
  }
  if (!entry.announced || entry.activated != activated) {
    entry.activated = activated;
    wlr_foreign_toplevel_handle_v1_set_activated(handle, activated);
  }
  if (!entry.announced || entry.minimized != client.minimized) {
    entry.minimized = client.minimized;
    wlr_foreign_toplevel_handle_v1_set_minimized(handle, client.minimized);
  }
  if (!entry.announced || entry.maximized != client.maximized) {
    entry.maximized = client.maximized;
    wlr_foreign_toplevel_handle_v1_set_maximized(handle, client.maximized);
  }
  if (!entry.announced || entry.fullscreen != client.fullscreen) {
    entry.fullscreen = client.fullscreen;
    wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, client.fullscreen);
  }
  entry.announced = true;
}

void ForeignToplevels::remove(int window) { entries_.erase(window); }

#else

struct ForeignToplevels::Entry {};
ForeignToplevels::ForeignToplevels(wlr_foreign_toplevel_manager_v1 *, Requests) {}
ForeignToplevels::~ForeignToplevels() = default;
void ForeignToplevels::update(const ClientWindow &, bool) {}
void ForeignToplevels::remove(int) {}

#endif

} // namespace LunaDash
