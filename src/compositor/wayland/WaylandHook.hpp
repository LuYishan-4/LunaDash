#pragma once

#include <wayland-server-core.h>

#include <cstddef>
#include <functional>
#include <utility>

namespace LunaDash {

// A wl_listener that calls a function, for listeners owned by plain objects
// (one per protocol handle). It detaches itself when destroyed, and the
// function may destroy the hook while it runs.
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
      auto *hook = reinterpret_cast<WaylandHook *>(
          reinterpret_cast<char *>(self) - offsetof(WaylandHook, listener));
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
