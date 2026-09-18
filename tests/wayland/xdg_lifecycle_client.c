#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

static struct wl_compositor *g_compositor = NULL;
static struct xdg_wm_base *g_wm_base = NULL;
static bool g_configured = false;

static void handle_wm_ping(void *data, struct xdg_wm_base *wm_base,
                           uint32_t serial) {
  (void)data;
  xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = handle_wm_ping,
};

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
  (void)data;
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    uint32_t bind_version = version < 4 ? version : 4;
    g_compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                    bind_version);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    uint32_t bind_version = version < 3 ? version : 3;
    g_wm_base =
        wl_registry_bind(registry, name, &xdg_wm_base_interface, bind_version);
    xdg_wm_base_add_listener(g_wm_base, &wm_base_listener, NULL);
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                                 uint32_t name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static void handle_xdg_surface_configure(void *data,
                                         struct xdg_surface *xdg_surface,
                                         uint32_t serial) {
  (void)data;
  xdg_surface_ack_configure(xdg_surface, serial);
  g_configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = handle_xdg_surface_configure,
};

int main(void) {
  struct wl_display *display = wl_display_connect(NULL);
  if (!display) {
    fputs("failed to connect to LunaDash Wayland socket\n", stderr);
    return 2;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  if (wl_display_roundtrip(display) < 0 || !g_compositor || !g_wm_base) {
    fputs("required wl_compositor/xdg_wm_base globals are missing\n", stderr);
    return 3;
  }

  struct wl_surface *surface = wl_compositor_create_surface(g_compositor);
  struct xdg_surface *xdg_surface =
      xdg_wm_base_get_xdg_surface(g_wm_base, surface);
  xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
  struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg_surface);

  /* Exercise the wlroots 0.20 lifecycle edge case: state requests may arrive
   * before the xdg_surface's first commit. The compositor must not schedule a
   * configure until that initial commit has initialized the role. */
  xdg_toplevel_set_maximized(toplevel);
  xdg_toplevel_set_fullscreen(toplevel, NULL);
  wl_surface_commit(surface);
  if (wl_display_roundtrip(display) < 0 || !g_configured) {
    fputs("xdg-toplevel did not receive its initial configure\n", stderr);
    return 4;
  }

  xdg_toplevel_destroy(toplevel);
  xdg_surface_destroy(xdg_surface);
  wl_surface_destroy(surface);
  if (wl_display_roundtrip(display) < 0) {
    fputs("compositor disconnected while destroying xdg-toplevel\n", stderr);
    return 5;
  }

  xdg_wm_base_destroy(g_wm_base);
  wl_compositor_destroy(g_compositor);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}
