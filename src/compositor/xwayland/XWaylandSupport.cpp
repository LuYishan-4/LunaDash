#include "compositor/xwayland/XWaylandSupport.hpp"

#ifdef __cplusplus
#define static
#define class class_
#define delete delete_
#define namespace namespace_
extern "C" {
#endif
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#if __has_include(<wlr/xwayland.h>)
#include <wlr/xwayland.h>
#define LUDASH_WLR_HAS_XWAYLAND 1
#elif __has_include(<wlr/xwayland/xwayland.h>)
#include <wlr/xwayland/xwayland.h>
#define LUDASH_WLR_HAS_XWAYLAND 1
#else
#define LUDASH_WLR_HAS_XWAYLAND 0
#endif
#ifdef __cplusplus
}
#undef namespace
#undef delete
#undef class
#undef static
#endif

#include <QFileInfo>
#include <QStandardPaths>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

XWaylandSupport::XWaylandSupport(QObject *parent) : QObject(parent) {}

XWaylandSupport::~XWaylandSupport() { stop(); }

bool XWaylandSupport::start(wl_display *display, wlr_compositor *compositor,
                            wlr_seat *seat,
                            const QProcessEnvironment &environment,
                            NewSurfaceCallback newSurface) {
  stop();
  stopping_ = false;
  environment_ = environment;
  newSurface_ = std::move(newSurface);
  seat_ = seat;
  error_.clear();
  xwmReady_ = false;

#if !LUDASH_WLR_HAS_XWAYLAND
  Q_UNUSED(display)
  Q_UNUSED(compositor)
  Q_UNUSED(seat)
  error_ = "This wlroots build does not provide XWayland support.";
  return false;
#else
  if (!display || !compositor || !seat) {
    error_ = "The compositor seat is unavailable for XWayland.";
    return false;
  }
  if (QStandardPaths::findExecutable("Xwayland").isEmpty()) {
    error_ = "Install Xwayland for X11 applications.";
    return false;
  }

  // wlroots owns the X server sockets and XWM. Lazy mode reserves DISPLAY now
  // and starts the Xwayland process only when an X11 client connects.
  xwayland_ = wlr_xwayland_create(display, compositor, true);
  if (!xwayland_) {
    error_ = "wlroots could not create the XWayland/XWM bridge.";
    return false;
  }

  xwayland_->data = this;
  wlr_xwayland_set_seat(xwayland_, seat_);
  attachListener(&xwayland_->events.ready, ready_, this, handleReady);
  attachListener(&xwayland_->events.new_surface, newSurfaceListener_, this,
                 handleNewSurface);
  return true;
#endif
}

void XWaylandSupport::handleReady(wl_listener *listener, void *) {
  auto *self = listenerOwner<XWaylandSupport>(listener);
  if (!self || !self->xwayland_)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  // XWM creates the X11 CLIPBOARD/PRIMARY/DnD bridge on ready. Point it at the
  // same seat used by wl_data_device and zwlr_data_control so X11 and Wayland
  // selections become one compositor-owned clipboard domain.
  wlr_xwayland_set_seat(self->xwayland_, self->seat_);
#endif
  self->xwmReady_ = true;
}

void XWaylandSupport::handleNewSurface(wl_listener *listener, void *data) {
  auto *self = listenerOwner<XWaylandSupport>(listener);
  if (!self || !data || !self->newSurface_)
    return;
  self->newSurface_(static_cast<wlr_xwayland_surface *>(data));
}

void XWaylandSupport::stop() {
  if (stopping_)
    return;
  stopping_ = true;
  const auto remaining = clients_;
  for (auto *client : remaining) {
    if (!client || client->state() == QProcess::NotRunning)
      continue;
    client->terminate();
    if (!client->waitForFinished(800)) {
      client->kill();
      client->waitForFinished(500);
    }
  }
  clients_.clear();
  detachListener(newSurfaceListener_);
  detachListener(ready_);
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_) {
    xwayland_->data = nullptr;
    wlr_xwayland_destroy(xwayland_);
  }
#endif
  xwayland_ = nullptr;
  seat_ = nullptr;
  xwmReady_ = false;
  newSurface_ = {};
}

bool XWaylandSupport::stopped() const { return xwayland_ == nullptr; }

bool XWaylandSupport::startServer(QString *error) {
  if (!xwayland_) {
    if (error)
      *error = error_.isEmpty() ? "XWayland is unavailable." : error_;
    return false;
  }
  // Lazy wlr_xwayland is intentionally not spawned here. Exporting DISPLAY is
  // enough: the first X11 connection starts the server and then XWM.
  return true;
}

bool XWaylandSupport::isHelperSurface(qint64) const { return false; }

QJsonObject XWaylandSupport::snapshot() const {
  QString display;
  bool running = false;
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_) {
    display = QString::fromLocal8Bit(xwayland_->display_name
                                         ? xwayland_->display_name
                                         : "");
    running = xwmReady_ || (xwayland_->server && xwayland_->server->pid > 0);
  }
#endif
  return {{"available", xwayland_ != nullptr && error_.isEmpty()},
          {"mode", "rootless-lazy-xwm"},
          {"running", running},
          {"rootWindowVisible", false},
          {"selectionBridge", xwmReady_},
          {"display", display},
          {"authority", QString()},
          {"error", error_}};
}

void XWaylandSupport::applyEnvironment(
    QProcessEnvironment &environment) const {
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_ && xwayland_->display_name && !error_.size()) {
    environment.insert("DISPLAY",
                       QString::fromLocal8Bit(xwayland_->display_name));
    environment.remove("XAUTHORITY");
    return;
  }
#endif
  environment.remove("DISPLAY");
  environment.remove("XAUTHORITY");
}

bool XWaylandSupport::launch(const QStringList &command, QString *error) {
  if (command.isEmpty() || command.size() > 64) {
    if (error)
      *error = "Enter an executable and its arguments.";
    return false;
  }
  const QString executable = QStandardPaths::findExecutable(command.first());
  if (executable.isEmpty() && !QFileInfo::exists(command.first())) {
    if (error)
      *error = "Executable not found: " + command.first();
    return false;
  }
  if (!startServer(error))
    return false;

  auto *process = new QProcess(this);
  auto environment = environment_;
  applyEnvironment(environment);
  environment.remove("WAYLAND_DISPLAY");
  environment.insert("QT_QPA_PLATFORM", "xcb");
  environment.insert("GDK_BACKEND", "x11");
  environment.insert("SDL_VIDEODRIVER", "x11");
  environment.insert("_JAVA_AWT_WM_NONREPARENTING", "1");
  environment.remove("LIBGL_ALWAYS_SOFTWARE");
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::ForwardedChannels);
  clients_ << process;
  connect(process, &QProcess::finished, this, [this, process] {
    clients_.removeAll(process);
    process->deleteLater();
  });
  process->start(executable, command.mid(1));
  return true;
}

} // namespace LunaDash
