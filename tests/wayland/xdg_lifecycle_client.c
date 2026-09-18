#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

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
  g_configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = handle_xdg_surface_configure,
};

static struct wl_buffer *create_buffer(void) {
  const int width = 96;
  const int height = 64;
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

  uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels == MAP_FAILED) {
    close(fd);
    return NULL;
  }
  for (size_t i = 0; i < size / sizeof(uint32_t); ++i)
    pixels[i] = 0xff315a7aU;
  munmap(pixels, size);

  struct wl_shm_pool *pool = wl_shm_create_pool(g_shm, fd, (int)size);
  struct wl_buffer *buffer =
      wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);
  return buffer;
}

int main(void) {
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

  struct wl_buffer *buffer = create_buffer();
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
