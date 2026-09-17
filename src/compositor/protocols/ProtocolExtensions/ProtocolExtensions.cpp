#include "compositor/protocols/ProtocolExtensions/ProtocolExtensions.hpp"
#include <QDebug>
#include <QPoint>
#include <QQuickWindow>
#include <QSize>
#include <QString>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandOutput>
#if __has_include(<QtWaylandCompositor/QWaylandXdgOutputManagerV1>)
#include <QtWaylandCompositor/QWaylandXdgOutputManagerV1>
#define LUNADASH_HAS_XDG_OUTPUT 1
#endif
#if __has_include(<QtWaylandCompositor/QWaylandIdleInhibitManagerV1>)
#include <QtWaylandCompositor/QWaylandIdleInhibitManagerV1>
#define LUNADASH_HAS_IDLE_INHIBIT 1
#endif
namespace LuDash {
void installCoreProtocolExtensions(QWaylandCompositor *compositor,
                                   QWaylandOutput *output,
                                   QQuickWindow *window) {
  if (!compositor)
    return;
  // xdg-output reports the output in logical coordinates. Capture tools and
  // monitors read the output name and geometry from it. Qt fails a client
  // request with a protocol error when the manager is announced without an
  // object for every output, so the object is created with the global.
#if defined(LUNADASH_HAS_XDG_OUTPUT)
  if (output) {
    auto *manager = new QWaylandXdgOutputManagerV1(compositor);
    auto *xdgOutput = new QWaylandXdgOutputV1(output, manager);
    xdgOutput->setName(QStringLiteral("LunaDash-1"));
    xdgOutput->setDescription(QStringLiteral("LunaDash desktop output"));
    xdgOutput->setLogicalPosition(QPoint(0, 0));
    const auto applyLogicalSize = [xdgOutput, output, window] {
      const QRect geometry = output->geometry();
      const QSize fallback = geometry.isValid() ? geometry.size() : QSize{};
      const QSize size =
          window && !window->size().isEmpty() ? window->size() : fallback;
      xdgOutput->setLogicalSize(size);
    };
    applyLogicalSize();
    if (window) {
      QObject::connect(window, &QQuickWindow::widthChanged, xdgOutput,
                       [applyLogicalSize] { applyLogicalSize(); });
      QObject::connect(window, &QQuickWindow::heightChanged, xdgOutput,
                       [applyLogicalSize] { applyLogicalSize(); });
    }
  }
#else
  qInfo(
      "LunaDash: xdg-output is unavailable in this Qt build; output names and "
      "logical geometry stay unannounced.");
#endif
  // idle-inhibit records a client's request to keep the session awake. LunaDash
  // has no idle blanking or locking yet, so every inhibition is satisfied; the
  // global exists so clients stop failing their startup capability check.
#if defined(LUNADASH_HAS_IDLE_INHIBIT)
  new QWaylandIdleInhibitManagerV1(compositor);
#else
  qInfo("LunaDash: idle-inhibit is unavailable in this Qt build.");
#endif
}
} // namespace LuDash
