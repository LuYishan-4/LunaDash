// Registration only: which components make up the wlroots runtime, in which
// order they start, and how the runtime is taken apart again. Handlers live in
// the components and per-object states; see Register.hpp.
#include "compositor/wayland/Register.hpp"
#include "compositor/input/ClipboardComponent.hpp"
#include "compositor/input/InputDeviceComponent.hpp"
#include "compositor/input/PointerComponent.hpp"
#include "compositor/input/TextInputComponent.hpp"
#include "compositor/wayland/LayerShellComponent.hpp"
#include "compositor/wayland/OutputComponent.hpp"
#include "compositor/wayland/WindowCaptureComponent.hpp"
#include "compositor/wayland/XdgShellComponent.hpp"
#include <utility>

namespace LunaDash {
namespace {
template <typename State> void destroyStates(std::vector<State *> &states) {
  for (auto *state : std::exchange(states, {}))
    delete state;
}
} // namespace

template <typename ComponentType>
bool WaylandCompositor::Impl::registerComponent() {
  static_assert(std::is_base_of_v<Component, ComponentType>,
                "registered types are Impl::Component");
  auto component = std::make_unique<ComponentType>(this);
  auto *registered = component.get();
  components.push_back(std::move(component));
  return registered->start();
}

bool WaylandCompositor::Impl::initialize() {
  wlr_log_init(WLR_INFO, nullptr);
  if (!createRuntime())
    return false;
  return registerComponent<OutputComponent>() &&
         registerComponent<InputDeviceComponent>() &&
         registerComponent<PointerComponent>() &&
         registerComponent<ClipboardComponent>() &&
         registerComponent<TextInputComponent>() &&
         registerComponent<XdgShellComponent>() &&
         registerComponent<LayerShellComponent>() &&
         registerComponent<WindowCaptureComponent>() && startRuntime();
}

void WaylandCompositor::Impl::shutdown() {
  releaseRuntime();
  if (!display)
    return;
  wl_display_destroy(display);
  display = nullptr;
  eventLoop = nullptr;
}

void WaylandCompositor::Impl::releaseRuntime() {
  cancelScheduledCallbacks();
  cancelScratchpadTimeout();
  clearPendingDisplay();
  if (!display || runtimeReleased)
    return;
  runtimeReleased = true;

  // Underlays retain renderer buffers and listeners on client scene nodes.
  // Release them while both the scene and renderer are still alive.
  windowGlass.reset();
  windowCorners.reset();
  // Windows and popups destroy their own states here.
  wl_display_destroy_clients(display);
  // Client resources reach back into the manager while they are destroyed.
  backgroundEffects.reset();

  // Components stop listening newest first.
  while (!components.empty())
    components.pop_back();

  // States of wlroots-owned objects (outputs, X11 windows, layers, devices)
  // disconnect before the scene and backend go, so late destruction cannot
  // call back into already-freed scene nodes.
  primaryOutput = nullptr;
  activeTextInput = nullptr;
  destroyStates(outputs);
  destroyStates(xwaylandSurfaces);
  destroyStates(layers);
  destroyStates(keyboards);
  destroyStates(textInputs);
  destroyStates(inputPopups);
  delete std::exchange(inputMethod, nullptr);

  destroyRuntime();
}

} // namespace LunaDash
