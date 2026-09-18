#pragma once

#include <QObject>
#include <memory>

class QKeyEvent;
class QQuickWindow;
class QWaylandCompositor;
class QWaylandOutput;

namespace LuDash {

class InputMethodSupport final : public QObject {
public:
  InputMethodSupport(QWaylandCompositor *compositor, QQuickWindow *window,
                     QWaylandOutput *output);
  ~InputMethodSupport() override;

  bool filterKeyEvent(QKeyEvent *event);
  bool hasKeyboardGrab() const;
  bool nativeBridgeAvailable() const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

InputMethodSupport *installInputMethodProtocols(QWaylandCompositor *compositor,
                                                QQuickWindow *window,
                                                QWaylandOutput *output);

} // namespace LuDash
