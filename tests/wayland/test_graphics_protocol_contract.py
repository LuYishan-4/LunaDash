"""Protect compositor protocols needed by modern GPU clients and screencast."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
register = (root / "src/compositor/wayland/Register.cpp").read_text(encoding="utf-8")
headers = (root / "src/compositor/wayland/wlroots/WlrootsHeaders.hpp").read_text(
    encoding="utf-8"
)
runtime = (root / "src/compositor/wayland/Register.hpp").read_text(encoding="utf-8")
output = (root / "src/compositor/wayland/Output.cpp").read_text(encoding="utf-8")
portal = (root / "data/portal/lunadash-portals.conf").read_text(encoding="utf-8")

# Chromium/Electron must negotiate buffers with the compositor instead of
# relying on per-application launch flags.
assert "wlr_linux_dmabuf_v1_create_with_renderer" in register
assert "wlr_scene_set_linux_dmabuf_v1" in register

# Newer wlroots/NVIDIA stacks use timeline fences. Keep this optional at build
# time for distro wlroots versions, but advertise it whenever both backend and
# renderer support timeline synchronization.
assert "wlr_linux_drm_syncobj_v1.h" in headers
assert "LUDASH_WLR_HAS_DRM_SYNCOBJ" in headers
assert "wlr_linux_drm_syncobj_manager_v1_create" in register
assert "renderer->features.timeline" in register
assert "backend->features.timeline" in register
assert "explicitSync" in runtime

# xdg-desktop-portal-wlr queues zwlr_screencopy frames. A request must keep
# driving output frames long enough for the next streaming request to arrive;
# otherwise consumers can display only the first captured frame.
assert "screencopyPendingForOutput" in output
assert "screencopyKeepalive" in output
frame = output[output.index("void WaylandCompositor::Impl::handleOutputFrame") :]
assert "capturePending" in frame
assert "wlr_output_schedule_frame(state->output)" in frame

# LunaDash owns FileChooser only; screencast and screenshots remain routed to
# the wlroots portal backend.
assert "org.freedesktop.impl.portal.ScreenCast=wlr" in portal
assert "org.freedesktop.impl.portal.Screenshot=wlr" in portal

print("Graphics protocol contract passed: dmabuf, explicit sync and live screencopy.")
