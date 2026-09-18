#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LuDashWlrBackend LuDashWlrBackend;
typedef struct LuDashWlrWindow LuDashWlrWindow;

typedef struct LuDashWlrRect {
  int x;
  int y;
  int width;
  int height;
} LuDashWlrRect;

typedef struct LuDashWlrOutputInfo {
  char name[128];
  int width;
  int height;
  int refresh_millihz;
  float scale;
  int nested;
  int fullscreen;
} LuDashWlrOutputInfo;

enum LuDashWlrModifier {
  LUDASH_WLR_MOD_META = 1u << 0,
  LUDASH_WLR_MOD_CONTROL = 1u << 1,
  LUDASH_WLR_MOD_ALT = 1u << 2,
  LUDASH_WLR_MOD_SHIFT = 1u << 3,
};

typedef struct LuDashWlrCallbacks {
  void *userdata;
  void (*window_created)(void *userdata, LuDashWlrWindow *window);
  void (*window_mapped)(void *userdata, LuDashWlrWindow *window);
  void (*window_unmapped)(void *userdata, LuDashWlrWindow *window);
  void (*window_destroyed)(void *userdata, LuDashWlrWindow *window);
  void (*window_metadata_changed)(void *userdata, LuDashWlrWindow *window);
  void (*window_focus_requested)(void *userdata, LuDashWlrWindow *window);
  void (*layout_changed)(void *userdata);
  int (*key_event)(void *userdata, uint32_t keysym, uint32_t modifiers,
                   int pressed, int keypad);
} LuDashWlrCallbacks;

LuDashWlrBackend *ludash_wlr_backend_create(
    const char *socket_name, const char *renderer_preference, int fullscreen,
    const LuDashWlrCallbacks *callbacks, char *error, size_t error_capacity);
int ludash_wlr_backend_start(LuDashWlrBackend *backend, char *error,
                             size_t error_capacity);
void ludash_wlr_backend_destroy(LuDashWlrBackend *backend);

int ludash_wlr_backend_event_fd(const LuDashWlrBackend *backend);
int ludash_wlr_backend_dispatch(LuDashWlrBackend *backend);
void ludash_wlr_backend_flush(LuDashWlrBackend *backend);

int ludash_wlr_backend_renderer_ready(const LuDashWlrBackend *backend);
int ludash_wlr_backend_seat_protocol_version(const LuDashWlrBackend *backend);
int ludash_wlr_backend_data_device_protocol_version(
    const LuDashWlrBackend *backend);
int ludash_wlr_backend_input_method_ready(const LuDashWlrBackend *backend);
int ludash_wlr_backend_layer_count(const LuDashWlrBackend *backend);

void ludash_wlr_backend_output_info(const LuDashWlrBackend *backend,
                                    LuDashWlrOutputInfo *info);
void ludash_wlr_backend_usable_area(const LuDashWlrBackend *backend,
                                    LuDashWlrRect *rect);
int ludash_wlr_backend_resize_nested(LuDashWlrBackend *backend, int width,
                                     int height);
void ludash_wlr_backend_schedule_frame(LuDashWlrBackend *backend);

int ludash_wlr_backend_set_keyboard_config(LuDashWlrBackend *backend,
                                           const char *layout, int repeat_rate,
                                           int repeat_delay, char *error,
                                           size_t error_capacity);
void ludash_wlr_backend_resend_modifiers(LuDashWlrBackend *backend);
int ludash_wlr_backend_send_key_name(LuDashWlrBackend *backend,
                                     const char *xkb_key_name,
                                     int with_control);

const char *ludash_wlr_window_title(const LuDashWlrWindow *window);
const char *ludash_wlr_window_app_id(const LuDashWlrWindow *window);
int64_t ludash_wlr_window_pid(const LuDashWlrWindow *window);
int ludash_wlr_window_is_mapped(const LuDashWlrWindow *window);
int ludash_wlr_window_has_parent(const LuDashWlrWindow *window);
void ludash_wlr_window_buffer_size(const LuDashWlrWindow *window, int *width,
                                   int *height);

void ludash_wlr_window_set_visible(LuDashWlrWindow *window, int visible);
void ludash_wlr_window_configure(LuDashWlrWindow *window, int x, int y,
                                 int width, int height);
void ludash_wlr_window_set_activated(LuDashWlrBackend *backend,
                                     LuDashWlrWindow *window, int activated);
void ludash_wlr_window_raise(LuDashWlrWindow *window);
void ludash_wlr_window_close(LuDashWlrWindow *window);
void ludash_wlr_window_set_maximized(LuDashWlrWindow *window, int maximized);

#ifdef __cplusplus
}
#endif
