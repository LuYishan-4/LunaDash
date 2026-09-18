#include "compositor/protocols/CoreProtocolCompat/CoreProtocolCompositor.hpp"

#include <QtWaylandCompositor/QWaylandSeat>

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandTouch>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>

// QWaylandSeatPrivate deliberately has no public/protected initializer that lets
// compositors select the wl_seat global version. LunaDash owns this seat
// implementation, so expose the private state only in this isolated translation
// unit instead of patching the system Qt package.
#define private public
#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#undef private
#endif

namespace LuDash {
namespace {

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
constexpr int kSeatProtocolVersion = 5;

class CoreProtocolSeat final : public QWaylandSeat {
public:
  explicit CoreProtocolSeat(QWaylandCompositor *compositor)
      : QWaylandSeat(compositor) {}

  void initialize() override {
    auto *d = QWaylandSeatPrivate::get(this);
    if (!d || d->isInitialized)
      return;

    // Qt 6.9 hard-codes version 4 here. Version 5 adds wl_seat.release and is
    // the minimum required by modern clients such as Zed. The pointer,
    // keyboard and touch resources remain Qt's proven implementations.
    d->init(d->compositor->display(), kSeatProtocolVersion);

    auto *compositorPrivate = QWaylandCompositorPrivate::get(d->compositor);
    if (d->capabilities & QWaylandSeat::Pointer)
      d->pointer.reset(compositorPrivate->callCreatePointerDevice(this));
    if (d->capabilities & QWaylandSeat::Touch)
      d->touch.reset(compositorPrivate->callCreateTouchDevice(this));
    if (d->capabilities & QWaylandSeat::Keyboard)
      d->keyboard.reset(compositorPrivate->callCreateKeyboardDevice(this));

    d->isInitialized = true;
  }
};
#endif

} // namespace

QWaylandSeat *CoreProtocolCompositor::createSeat() {
#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
  return new CoreProtocolSeat(this);
#else
  // Some distribution Qt packages ship a broken WaylandCompositorPrivate CMake
  // target without the matching private headers. Keep those builds functional;
  // Arch/full Qt development installations take the v5 path above.
  return QWaylandQuickCompositor::createSeat();
#endif
}

} // namespace LuDash
