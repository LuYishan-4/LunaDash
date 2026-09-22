#pragma once

// wlroots is a C API. Some supported releases expose C99-only declarations
// such as array[static N] and struct members named delete/namespace.
// Keep those lexical incompatibilities inside this one compatibility boundary;
// the macros are undefined immediately after the C headers are parsed.
#ifdef __cplusplus
#define static
#define delete delete_
#define namespace namespace_
extern "C" {
#endif

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#if __has_include(<wlr/types/wlr_buffer.h>)
#include <wlr/types/wlr_buffer.h>
#endif
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#if __has_include(<wlr/types/wlr_foreign_toplevel_management_v1.h>)
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#define LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT 1
#else
#define LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT 0
#endif
#if __has_include(<wlr/types/wlr_ext_foreign_toplevel_list_v1.h>) && \
    __has_include(<wlr/types/wlr_ext_image_capture_source_v1.h>) && \
    __has_include(<wlr/types/wlr_ext_image_copy_capture_v1.h>)
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#define LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE 1
#else
#define LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE 0
#endif
#if __has_include(<wlr/types/wlr_data_control_v1.h>)
#include <wlr/types/wlr_data_control_v1.h>
#define LUDASH_WLR_HAS_DATA_CONTROL 1
#else
#define LUDASH_WLR_HAS_DATA_CONTROL 0
#endif
#if __has_include(<wlr/types/wlr_drm.h>)
#include <wlr/types/wlr_drm.h>
#endif
#include <wlr/types/wlr_idle_inhibit_v1.h>
#if __has_include(<wlr/types/wlr_linux_dmabuf_v1.h>)
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#endif
#if __has_include(<wlr/types/wlr_linux_drm_syncobj_v1.h>)
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#define LUDASH_WLR_HAS_DRM_SYNCOBJ 1
#else
#define LUDASH_WLR_HAS_DRM_SYNCOBJ 0
#endif
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#if __has_include(<wlr/types/wlr_single_pixel_buffer_v1.h>)
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#endif
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#if __has_include(<wlr/xwayland.h>)
#include <wlr/xwayland.h>
#define LUDASH_WLR_HAS_XWAYLAND 1
#elif __has_include(<wlr/xwayland/xwayland.h>)
#include <wlr/xwayland/xwayland.h>
#define LUDASH_WLR_HAS_XWAYLAND 1
#else
#define LUDASH_WLR_HAS_XWAYLAND 0
#endif
#include <wlr/util/log.h>
#include <wlr/version.h>
#if __has_include(<wlr/util/transform.h>)
#include <wlr/util/transform.h>
#endif
#include <xkbcommon/xkbcommon.h>

#ifdef __cplusplus
}
#undef namespace
#undef delete
#undef static
#endif
