#pragma once
#include <QPointer>
#include <QSize>
#include <QString>
class QWaylandXdgToplevel;
class QWaylandQuickShellSurfaceItem;
namespace LuDash {
class WindowFrame;
class BlurItem;
struct ClientWindow {
  QPointer<QWaylandXdgToplevel> toplevel;
  QPointer<QWaylandQuickShellSurfaceItem> item;
  WindowFrame *frame = nullptr;
  BlurItem *blur = nullptr;
  bool presented = false;
  int workspace = 0;
  int id = 0;
  bool floating = false;
  bool desktop = false;
  bool mapped = false;
  bool minimized = false;
  bool maximized = false;
  bool initialRuleApplied = false;
  bool revealed = false;
  bool utility = false;
  bool lastConfiguredMaximized = false;
  QString appId;
  QString iconName;
  QSize lastSize;
};
} // namespace LuDash
