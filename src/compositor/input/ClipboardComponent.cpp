#include "compositor/input/ClipboardComponent.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::ClipboardComponent::start() {
  wlr_data_device_manager_create(impl->display);
#if LUDASH_WLR_HAS_DATA_CONTROL
  // Clipboard managers such as wl-paste --watch need the privileged
  // wlr-data-control protocol; wl_data_device alone is focus-bound and cannot
  // provide global clipboard history.
  impl->dataControl = wlr_data_control_manager_v1_create(impl->display);
  if (!impl->dataControl)
    return impl->fail("wlroots could not create the data-control manager.");
#endif
  listen<&ClipboardComponent::onRequestSelection>(
      &impl->seat->events.request_set_selection, requestSelection);
  return true;
}

void WaylandCompositor::Impl::ClipboardComponent::onRequestSelection(
    wlr_seat_request_set_selection_event *event) {
  if (event)
    wlr_seat_set_selection(impl->seat, event->source, event->serial);
}

} // namespace LunaDash
