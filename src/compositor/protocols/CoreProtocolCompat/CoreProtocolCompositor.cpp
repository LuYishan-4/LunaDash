#include "compositor/protocols/CoreProtocolCompat/CoreProtocolCompositor.hpp"
#include "compositor/protocols/CoreProtocolCompat/CoreDataDeviceCompat.hpp"

#include <QtWaylandCompositor/QWaylandSeat>

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
#include <QPointer>
#include <QtCore/private/qobject_p.h>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandTouch>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>

// Qt exposes no public hook for choosing wl_seat's global version or emitting
// the v5 pointer frame events. Keep all private-API access isolated to this
// compatibility translation unit so the rest of LunaDash stays on public Qt
// APIs.
#define private public
#include <QtWaylandCompositor/private/qwaylandpointer_p.h>
#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#undef private

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#endif

namespace LuDash {
namespace {

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
constexpr int kSeatProtocolVersion = 5;
constexpr int kPointerFrameVersion = 5;

class CoreProtocolPointer final : public QWaylandPointer {
public:
  explicit CoreProtocolPointer(QWaylandSeat *seat) : QWaylandPointer(seat) {}

  uint sendMousePressEvent(Qt::MouseButton button) override {
    const uint serial = QWaylandPointer::sendMousePressEvent(button);
    sendFrameForSurface(pointerPrivate()->enteredSurface);
    return serial;
  }

  uint sendMouseReleaseEvent(Qt::MouseButton button) override {
    const uint serial = QWaylandPointer::sendMouseReleaseEvent(button);
    sendFrameForSurface(pointerPrivate()->enteredSurface);
    return serial;
  }

  void sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos,
                          const QPointF &outputSpacePos) override {
    auto *d = pointerPrivate();
    QPointer<QWaylandSurface> previous = d->enteredSurface;

    QWaylandPointer::sendMouseMoveEvent(view, localPos, outputSpacePos);

    QWaylandSurface *current = d->enteredSurface.data();
    if (previous && previous != current)
      sendFrameForClient(previous->waylandClient());
    if (current)
      sendFrameForClient(current->waylandClient());
  }

  void sendMouseWheelEvent(Qt::Orientation orientation, int delta) override {
    auto *d = pointerPrivate();
    if (!d->enteredSurface || delta == 0)
      return;

    const uint32_t time = d->compositor()->currentTimeMsecs();
    const uint32_t axis =
        orientation == Qt::Horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                      : WL_POINTER_AXIS_VERTICAL_SCROLL;
    const int discrete = -delta / 120;
    const wl_fixed_t value = wl_fixed_from_double(-delta / 12.0);

    const auto resources =
        d->resourceMap().values(d->enteredSurface->waylandClient());
    for (auto *resource : resources) {
      const int version = wl_resource_get_version(resource->handle);
      if (version >= kPointerFrameVersion) {
        d->send_axis_source(resource->handle, WL_POINTER_AXIS_SOURCE_WHEEL);
        if (discrete != 0)
          d->send_axis_discrete(resource->handle, axis, discrete);
      }
      d->send_axis(resource->handle, time, axis, value);
      if (version >= kPointerFrameVersion)
        d->send_frame(resource->handle);
    }
  }

private:
  QWaylandPointerPrivate *pointerPrivate() const {
    return static_cast<QWaylandPointerPrivate *>(
        QObjectPrivate::get(const_cast<CoreProtocolPointer *>(this)));
  }

  void sendFrameForSurface(QWaylandSurface *surface) {
    if (surface)
      sendFrameForClient(surface->waylandClient());
  }

  void sendFrameForClient(wl_client *client) {
    if (!client)
      return;
    auto *d = pointerPrivate();
    const auto resources = d->resourceMap().values(client);
    for (auto *resource : resources)
      if (wl_resource_get_version(resource->handle) >= kPointerFrameVersion)
        d->send_frame(resource->handle);
  }
};

class CoreProtocolSeat final : public QWaylandSeat {
public:
  explicit CoreProtocolSeat(QWaylandCompositor *compositor)
      : QWaylandSeat(compositor) {}

  void initialize() override {
    auto *d = QWaylandSeatPrivate::get(this);
    if (!d || d->isInitialized)
      return;

    // Qt 6.9 hard-codes version 4. Version 5 is required by Zed and changes
    // pointer delivery semantics: clients may wait for wl_pointer.frame before
    // dispatching a logical input batch. The CoreProtocolPointer above supplies
    // those v5 frames and wheel metadata.
    d->init(d->compositor->display(), kSeatProtocolVersion);

    auto *compositorPrivate = QWaylandCompositorPrivate::get(d->compositor);
    if (d->capabilities & QWaylandSeat::Pointer)
      d->pointer.reset(compositorPrivate->callCreatePointerDevice(this));
    if (d->capabilities & QWaylandSeat::Touch)
      d->touch.reset(compositorPrivate->callCreateTouchDevice(this));
    if (d->capabilities & QWaylandSeat::Keyboard)
      d->keyboard.reset(compositorPrivate->callCreateKeyboardDevice(this));

    d->isInitialized = true;
    qInfo("LunaDash core protocol: wl_seat v5 + wl_pointer v5 framing");
  }
};
#endif

} // namespace

CoreProtocolCompositor::~CoreProtocolCompositor() {
  delete coreDataDeviceManager_;
  coreDataDeviceManager_ = nullptr;
}

void CoreProtocolCompositor::create() {
  QWaylandQuickCompositor::create();
#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
  coreDataDeviceManager_ = installCoreDataDeviceV3(this);
#endif
}

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

QWaylandPointer *
CoreProtocolCompositor::createPointerDevice(QWaylandSeat *seat) {
#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
  return new CoreProtocolPointer(seat);
#else
  return QWaylandQuickCompositor::createPointerDevice(seat);
#endif
}

} // namespace LuDash
