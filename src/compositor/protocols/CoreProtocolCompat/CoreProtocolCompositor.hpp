#pragma once

#include <QtWaylandCompositor/QWaylandQuickCompositor>

namespace LuDash {

// Qt 6.9 still advertises wl_seat v4 from QWaylandSeat::initialize(), while
// current native Wayland clients such as Zed require wl_seat v5. Keep the rest
// of QtWaylandCompositor's input plumbing, but create a LunaDash-owned seat
// whose protocol global is initialized at v5.
class CoreProtocolCompositor final : public QWaylandQuickCompositor {
public:
  using QWaylandQuickCompositor::QWaylandQuickCompositor;

  static constexpr int seatProtocolVersion() noexcept {
#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
    return 5;
#else
    return 4;
#endif
  }

protected:
  QWaylandSeat *createSeat() override;
};

} // namespace LuDash
