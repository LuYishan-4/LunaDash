#pragma once

#include <wayland-server-core.h>

#include <cstddef>
#include <functional>
#include <utility>

namespace LunaDash {

struct WaylandHook {
  wl_listener listener{};
  std::function<void(void *)> run;

  WaylandHook() { wl_list_init(&listener.link); }
  ~WaylandHook() { detach(); }
  WaylandHook(const WaylandHook &) = delete;
  WaylandHook &operator=(const WaylandHook &) = delete;

  void attach(wl_signal *signal, std::function<void(void *)> function) {
    detach();
    run = std::move(function);
    listener.notify = [](wl_listener *self, void *data) {
      WaylandHook *hook = wl_container_of(self, hook, listener);
      auto function = hook->run;  // `run` may destroy the hook
      function(data);
    };
    wl_signal_add(signal, &listener);
  }
  void detach() {
    wl_list_remove(&listener.link);
    wl_list_init(&listener.link);
  }
};

} // namespace LunaDash
