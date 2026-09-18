#pragma once

#include <QtWaylandCompositor/QWaylandQuickCompositor>

namespace LuDash {

// Qt 6.9 still advertises wl_seat v4 from QWaylandSeat::initialize(), while
// current native Wayland clients such as Zed require wl_seat v5. LunaDash
// keeps Qt's focus/keymap plumbing but owns the seat version and pointer v5
// event framing so clients get real v5 semantics rather than a version-number
// bump only.
class CoreProtocolCompositor final : public QWaylandQuickCompositor {
public:
  using QWaylandQuickCompositor::QWaylandQuickCompositor;

  void create() override;

  static constexpr int seatProtocolVersion() noexcept {
#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
    return 5;
#else
    return 4;
#endif
  }

protected:
  QWaylandSeat *createSeat() override;
  QWaylandPointer *createPointerDevice(QWaylandSeat *seat) override;
};

} // namespace LuDash
