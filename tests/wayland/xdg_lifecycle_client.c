#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include <wayland-client.h>

static struct wl_compositor *g_compositor = NULL;
static struct wl_shm *g_shm = NULL;
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
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    g_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
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
  if (data)
    *(bool *)data = true;
  else
    g_configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = handle_xdg_surface_configure,
};

static struct wl_buffer *create_buffer(int width, int height) {
  const int stride = width * 4;
  const size_t size = (size_t)stride * (size_t)height;
  char path[] = "/tmp/ludash-xdg-buffer-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0)
    return NULL;
  unlink(path);
  if (ftruncate(fd, (off_t)size) != 0) {
    close(fd);
    return NULL;
  }

  uint32_t *pixels =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels == MAP_FAILED) {
    close(fd);
    return NULL;
  }
  for (size_t i = 0; i < size / sizeof(uint32_t); ++i)
    pixels[i] = 0xff315a7aU;
  munmap(pixels, size);

  struct wl_shm_pool *pool = wl_shm_create_pool(g_shm, fd, (int)size);
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(
      pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);
  return buffer;
}

struct popup_fixture {
  struct wl_surface *surface;
  struct xdg_surface *xdg;
  struct xdg_popup *popup;
  struct wl_buffer *buffer;
  bool configured;
  bool framed;
  bool repositioned;
};

static void popup_configure(void *data, struct xdg_popup *popup, int32_t x,
                            int32_t y, int32_t width, int32_t height) {
  (void)data;
  (void)popup;
  (void)x;
  (void)y;
  if (width <= 0 || height <= 0)
    abort();
}
static void popup_done(void *data, struct xdg_popup *popup) {
  (void)data;
  (void)popup;
}
static void popup_repositioned(void *data, struct xdg_popup *popup,
                               uint32_t token) {
  (void)popup;
  ((struct popup_fixture *)data)->repositioned = token == 77;
}
static const struct xdg_popup_listener popup_listener = {
    .configure = popup_configure,
    .popup_done = popup_done,
    .repositioned = popup_repositioned,
};
static void popup_frame(void *data, struct wl_callback *callback,
                        uint32_t time) {
  (void)time;
  ((struct popup_fixture *)data)->framed = true;
  wl_callback_destroy(callback);
}
static const struct wl_callback_listener frame_listener = {.done = popup_frame};

static int map_popup(struct wl_display *display, struct xdg_surface *parent,
                     struct popup_fixture *fixture) {
  fixture->surface = wl_compositor_create_surface(g_compositor);
  fixture->xdg = xdg_wm_base_get_xdg_surface(g_wm_base, fixture->surface);
  xdg_surface_add_listener(fixture->xdg, &xdg_surface_listener,
                           &fixture->configured);
  struct xdg_positioner *positioner = xdg_wm_base_create_positioner(g_wm_base);
  xdg_positioner_set_size(positioner, 32, 24);
  xdg_positioner_set_anchor_rect(positioner, 8, 8, 1, 1);
  xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
  xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
  xdg_positioner_set_constraint_adjustment(
      positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y);
  fixture->popup = xdg_surface_get_popup(fixture->xdg, parent, positioner);
  xdg_popup_add_listener(fixture->popup, &popup_listener, fixture);
  wl_surface_commit(fixture->surface);
  while (!fixture->configured)
    if (wl_display_dispatch(display) < 0)
      return 1;
  fixture->buffer = create_buffer(32, 24);
  if (!fixture->buffer)
    return 2;
  wl_callback_add_listener(wl_surface_frame(fixture->surface), &frame_listener,
                           fixture);
  wl_surface_attach(fixture->surface, fixture->buffer, 0, 0);
  wl_surface_damage(fixture->surface, 0, 0, 32, 24);
  wl_surface_commit(fixture->surface);
  // A frame callback proves the popup is present in the rendered scene, not
  // merely configured. This failed when LunaDash ignored xdg_popup roles.
  while (!fixture->framed)
    if (wl_display_dispatch(display) < 0)
      return 3;
  xdg_positioner_set_offset(positioner, 4, 4);
  xdg_popup_reposition(fixture->popup, positioner, 77);
  while (!fixture->repositioned)
    if (wl_display_dispatch(display) < 0)
      return 4;
  xdg_positioner_destroy(positioner);
  return 0;
}
static void destroy_popup(struct popup_fixture *fixture) {
  xdg_popup_destroy(fixture->popup);
  xdg_surface_destroy(fixture->xdg);
  wl_surface_destroy(fixture->surface);
  wl_buffer_destroy(fixture->buffer);
}

int main(void) {
  alarm(10);
  struct wl_display *display = wl_display_connect(NULL);
  if (!display) {
    fputs("failed to connect to LunaDash Wayland socket\n", stderr);
    return 2;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  if (wl_display_roundtrip(display) < 0 || !g_compositor || !g_wm_base ||
      !g_shm) {
    fputs("required wl_compositor/wl_shm/xdg_wm_base globals are missing\n",
          stderr);
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

  struct wl_buffer *buffer = create_buffer(96, 64);
  if (!buffer) {
    fputs("could not create lifecycle test shm buffer\n", stderr);
    return 5;
  }
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, 96, 64);
  wl_surface_commit(surface);
  if (wl_display_roundtrip(display) < 0) {
    fputs("compositor disconnected while mapping xdg-toplevel\n", stderr);
    return 6;
  }

  struct popup_fixture menu = {0}, submenu = {0};
  if (map_popup(display, xdg_surface, &menu) != 0 ||
      map_popup(display, menu.xdg, &submenu) != 0) {
    fputs("popup/menu rendering or reposition failed\n", stderr);
    return 8;
  }
  destroy_popup(&submenu);
  destroy_popup(&menu);
  if (wl_display_roundtrip(display) < 0)
    return 9;
  puts("Popup and nested submenu configured, rendered and repositioned");

  /* Destroy a genuinely mapped surface. This drives LunaDash's unmap path,
   * text-input focus cleanup and retained close-animation snapshot. */
  xdg_toplevel_destroy(toplevel);
  xdg_surface_destroy(xdg_surface);
  wl_surface_destroy(surface);
  if (wl_display_roundtrip(display) < 0) {
    fputs("compositor disconnected while destroying xdg-toplevel\n", stderr);
    return 7;
  }

  wl_buffer_destroy(buffer);
  xdg_wm_base_destroy(g_wm_base);
  wl_shm_destroy(g_shm);
  wl_compositor_destroy(g_compositor);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}
