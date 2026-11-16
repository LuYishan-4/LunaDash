#include "compositor/wayland/LayerShellComponent.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::LayerShellComponent::start() {
  impl->layerShell = wlr_layer_shell_v1_create(impl->display, 4);
  if (!impl->layerShell)
    return impl->fail("wlroots could not create the layer-shell global.");
  listen<&LayerShellComponent::onNewSurface>(
      &impl->layerShell->events.new_surface, newSurface);

  // Optional: shells opt their layer surfaces into the scene blur with it.
  auto *runtime = impl;
  impl->backgroundEffects =
      std::make_unique<BackgroundEffectManager>(impl->display, [runtime] {
        for (const auto *output : runtime->outputs)
          wlr_output_schedule_frame(output->output);
      });
  if (!impl->backgroundEffects->valid()) {
    logWarning("wlroots could not create the ext-background-effect-v1 global.");
    impl->backgroundEffects.reset();
  }
  return true;
}

void WaylandCompositor::Impl::LayerShellComponent::onNewSurface(
    wlr_layer_surface_v1 *surface) {
  if (!surface)
    return;
  if (!surface->output)
    surface->output = impl->primaryOutput;

  wlr_scene_tree *parent = impl->topLayer;
  switch (surface->pending.layer) {
  case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
    parent = impl->backgroundLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
    parent = impl->bottomLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
    parent = impl->topLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
    parent = impl->overlayLayer;
    break;
  }

  auto *sceneLayer = wlr_scene_layer_surface_v1_create(parent, surface);
  if (!sceneLayer)
    return;
  impl->layers.push_back(new LayerState(impl, surface, sceneLayer));
}

} // namespace LunaDash
