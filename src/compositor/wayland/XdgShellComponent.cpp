#include "compositor/wayland/XdgShellComponent.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/client/ClientWindow.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::XdgShellComponent::start() {
  impl->xdgShell = wlr_xdg_shell_create(impl->display, 3);
  if (!impl->xdgShell)
    return impl->fail("wlroots could not create the xdg-shell global.");
#if WLR_VERSION_MINOR < 18
  listen<&XdgShellComponent::onNewSurface>(&impl->xdgShell->events.new_surface,
                                           newSurface);
#else
  // Since wlroots 0.18, xdg_shell exposes role-specific new_toplevel events.
  // Track the role as soon as it is created, then wait for initial_commit.
  listen<&XdgShellComponent::onNewToplevel>(
      &impl->xdgShell->events.new_toplevel, newSurface);
  listen<&XdgShellComponent::onNewPopup>(&impl->xdgShell->events.new_popup,
                                         newPopup);
#endif

#if LUDASH_WLR_HAS_XDG_ACTIVATION
  // xdg-activation is optional in older wlroots builds. Its requests are
  // filtered through the seat's event-serial history before focus changes.
  impl->xdgActivation = wlr_xdg_activation_v1_create(impl->display);
  if (impl->xdgActivation)
    listen<&XdgShellComponent::onActivationRequest>(
        &impl->xdgActivation->events.request_activate, activationRequest);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  // Keep the wlroots toplevel-management compatibility global alongside the
  // newer ext-foreign-toplevel-list protocol. Native Wayland applications
  // such as GPU Screen Recorder UI still probe this interface for the active
  // toplevel and its title/app-id.
  impl->foreignToplevelManager =
      wlr_foreign_toplevel_manager_v1_create(impl->display);
  if (!impl->foreignToplevelManager)
    return impl->fail("wlroots could not create foreign-toplevel management.");
  // Taskbars and docks (the shell's included) focus, close and minimize windows here.
  auto *q = impl->q;
  const auto withClient = [q](int id, auto &&action) {
    if (auto *client = q->clientById(id))
      action(client);
  };
  impl->foreignToplevels = std::make_unique<ForeignToplevels>(
      impl->foreignToplevelManager,
      ForeignToplevels::Requests{
          [q](int id) { q->activateTask(id); },
          [withClient](int id) {
            withClient(id, [](ClientWindow *client) { closeClient(client); });
          },
          [q, withClient](int id, bool minimized) {
            if (!minimized) {
              q->activateTask(id);
              return;
            }
            withClient(id, [q](ClientWindow *client) { q->minimizeClient(client); });
          },
          [q, withClient](int id, bool maximized) {
            withClient(id, [q, maximized](ClientWindow *client) {
              q->setMaximized(client, maximized);
              q->arrange();
            });
          },
          [q, withClient](int id, bool fullscreen) {
            withClient(id, [q, fullscreen](ClientWindow *client) {
              q->setFullscreen(client, fullscreen);
            });
          }});
#endif
  return true;
}

#if WLR_VERSION_MINOR < 18
void WaylandCompositor::Impl::XdgShellComponent::onNewSurface(
    wlr_xdg_surface *surface) {
  if (!surface)
    return;
  if (surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && surface->toplevel)
    impl->addXdgToplevel(surface, surface->toplevel);
  else if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP && surface->popup)
    impl->addXdgPopup(surface->popup);
}
#else
void WaylandCompositor::Impl::XdgShellComponent::onNewToplevel(
    wlr_xdg_toplevel *toplevel) {
  if (toplevel && toplevel->base)
    impl->addXdgToplevel(toplevel->base, toplevel);
}

void WaylandCompositor::Impl::XdgShellComponent::onNewPopup(
    wlr_xdg_popup *popup) {
  impl->addXdgPopup(popup);
}
#endif

#if LUDASH_WLR_HAS_XDG_ACTIVATION
void WaylandCompositor::Impl::XdgShellComponent::onActivationRequest(
    wlr_xdg_activation_v1_request_activate_event *request) {
  if (!request || !request->token || !request->surface ||
      request->token->seat != impl->seat || !request->token->surface ||
      request->token->serial == 0 || !request->token->surface->resource)
    return;

  // A valid token must refer to an input event known to this seat client. This
  // accepts activation from a focused application or an interactive panel while
  // rejecting stale and fabricated serials from unrelated Wayland clients.
  auto *sourceClient = wlr_seat_client_for_wl_client(
      impl->seat, wl_resource_get_client(request->token->surface->resource));
  if (!sourceClient || !wlr_seat_client_validate_event_serial(
                           sourceClient, request->token->serial))
    return;

  auto *target = impl->clientForSurface(request->surface);
  if (target && target->mapped && !target->minimized && !target->utility)
    impl->q->focus(target);
}
#endif

} // namespace LunaDash
