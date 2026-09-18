#include "compositor/protocols/CoreProtocolCompat/CoreProtocolCompositor.hpp"

#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandTouch>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>

// QWaylandSeatPrivate deliberately has no public/protected initializer that lets
// compositors select the wl_seat global version. LunaDash owns this seat
// implementation, so expose the private state only in this isolated translation
// unit instead of patching the system Qt package.
#define private public
#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#undef private

namespace LuDash {
namespace {

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

} // namespace

QWaylandSeat *CoreProtocolCompositor::createSeat() {
  return new CoreProtocolSeat(this);
}

} // namespace LuDash
