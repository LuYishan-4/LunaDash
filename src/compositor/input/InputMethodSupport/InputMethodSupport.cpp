#include "compositor/input/InputMethodSupport/InputMethodSupport.hpp"
#include <QtWaylandCompositor/QWaylandTextInputManager>
#if __has_include(<QtWaylandCompositor/QWaylandTextInputManagerV3>)
#include <QtWaylandCompositor/QWaylandTextInputManagerV3>
#endif
#include <QtWaylandCompositor/QWaylandQtTextInputMethodManager>
#include <QDebug>
namespace LuDash {
void installInputMethodProtocols(QWaylandCompositor* compositor) {
    // Qt clients prefer this extension. Announce it first so older clients
    // do not discard a v2 input object while its initial events are queued.
    new QWaylandQtTextInputMethodManager(compositor);
#if __has_include(<QtWaylandCompositor/QWaylandTextInputManagerV3>)
    new QWaylandTextInputManagerV3(compositor);
#else
    qInfo("LuDash: text-input v3 is unavailable in this Qt build; using text-input v2 and Qt input-method protocols.");
#endif
    new QWaylandTextInputManager(compositor);
}
}
