#pragma once
#include <QJsonObject>
#include <QList>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <functional>

namespace LunaDash {

using LayoutWindowId = quint64;
using LayoutWorkspaceId = quint64;

struct WindowPlacement {
  LayoutWindowId window = 0;
  int width = 0;
  bool minimized = false;
  bool focused = false;
  QRect geometry;
  QJsonObject metadata;
  bool hiddenByMaximize = false;
  bool newWindow = false;
};

struct WorkspaceLayoutSnapshot {
  LayoutWorkspaceId workspace = 0;
  LayoutWindowId focusedWindow = 0;
  QList<WindowPlacement> columns;
  LayoutWindowId maximizedWindow = 0;
};

// Generic window-layout state contract. Layout type, editable settings and
// interaction policy belong to WindowTemplate, not this interface.
class WindowLayout {
public:
  virtual ~WindowLayout();
  using PlacementFilter = std::function<QList<WindowPlacement>(
      LayoutWorkspaceId, QRect, const QList<WindowPlacement> &)>;
  void setPlacementFilter(PlacementFilter filter);

  virtual void configure(const QJsonObject &settings) = 0;
  virtual bool insert(LayoutWorkspaceId workspace, LayoutWindowId window,
                      QSize preferredSize = {}) = 0;
  virtual bool remove(LayoutWindowId window) = 0;
  virtual bool setMinimized(LayoutWindowId window, bool minimized) = 0;
  virtual bool setMaximized(LayoutWindowId window, bool maximized) = 0;
  virtual bool moveToWorkspace(LayoutWindowId window,
                               LayoutWorkspaceId workspace) = 0;
  virtual bool focus(LayoutWindowId window) = 0;

  // Strategy-specific interaction is dispatched by action ID. The generic
  // layout contract intentionally does not encode tiling concepts such as
  // directional focus, groups, columns or resize directions.
  virtual bool performAction(const QString &action,
                             const QJsonObject &payload) = 0;

  virtual QList<WindowPlacement> layout(LayoutWorkspaceId workspace,
                                        QRect area) = 0;
  virtual QList<WindowPlacement> presentation(LayoutWorkspaceId workspace,
                                              QRect area) = 0;
  virtual WorkspaceLayoutSnapshot
  snapshot(LayoutWorkspaceId workspace) const = 0;

protected:
  QList<WindowPlacement> filterPlacements(
      LayoutWorkspaceId workspace, QRect area,
      const QList<WindowPlacement> &placements) const;

private:
  PlacementFilter placementFilter_;
};

} // namespace LunaDash
