#pragma once

#include "core/templates/WaylandSlot.hpp"
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <functional>

struct wl_display;
struct wlr_compositor;
struct wlr_seat;
struct wlr_xwayland;
struct wlr_xwayland_surface;

namespace LunaDash {

class XWaylandSupport final : public QObject {
public:
  using NewSurfaceCallback = std::function<void(wlr_xwayland_surface *)>;

  explicit XWaylandSupport(QObject *parent = nullptr);
  ~XWaylandSupport() override;

  bool start(wl_display *display, wlr_compositor *compositor, wlr_seat *seat,
             const QProcessEnvironment &environment,
             NewSurfaceCallback newSurface);
  void stop();
  bool stopped() const;
  bool startServer(QString *error = nullptr);
  bool isHelperSurface(qint64 processId) const;
  QJsonObject snapshot() const;
  void applyEnvironment(QProcessEnvironment &environment) const;
  bool launch(const QStringList &command, QString *error);

private:
  using Slot = Templates::WaylandSlot<XWaylandSupport>;
  static void handleReady(wl_listener *listener, void *data);
  static void handleNewSurface(wl_listener *listener, void *data);

  wlr_xwayland *xwayland_ = nullptr;
  wlr_seat *seat_ = nullptr;
  QList<QProcess *> clients_;
  QProcessEnvironment environment_;
  NewSurfaceCallback newSurface_;
  Slot ready_;
  Slot newSurfaceListener_;
  QString error_;
  bool stopping_ = false;
  bool xwmReady_ = false;
};

} // namespace LunaDash
