#pragma once
class QQuickWindow;
class QWaylandCompositor;
class QWaylandOutput;
namespace LuDash {
// Announce the optional protocol globals that describe the single output and
// record a client's request to stay awake. The created extension objects are
// owned by the compositor and live for its lifetime, like the built-in
// xdg-shell and viewporter globals.
void installCoreProtocolExtensions(QWaylandCompositor *compositor,
                                   QWaylandOutput *output,
                                   QQuickWindow *window);
} // namespace LuDash
