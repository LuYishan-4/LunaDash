#include "desktop/system/SystemStatus.hpp"

#include <wayland-server-core.h>

#include <cassert>
#include <cstdint>

int main() {
  auto *display = wl_display_create();
  assert(display);
  auto *eventLoop = wl_display_get_event_loop(display);
  assert(eventLoop);

  {
    LunaDash::SystemStatus status(eventLoop);
    const auto snapshot = status.snapshot();
    for (const auto *key :
         {"os", "kernel", "architecture", "host", "user", "displayName",
          "avatar", "cpuPercent", "cpuHistory", "memoryUsed", "memoryTotal",
          "memoryPercent", "gpuAvailable", "gpuPercent"})
      assert(snapshot.contains(key));
    assert(snapshot.at("cpuHistory").is_array());
    assert(snapshot.at("cpuHistory").size() <= 20);
    assert(snapshot.at("memoryPercent").is_number_integer());
  }

  wl_display_destroy(display);
}
