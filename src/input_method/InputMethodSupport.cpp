#include <LuDash/input_method/InputMethodSupport.h>
#include <QtWaylandCompositor/QWaylandTextInputManager>
#include <QtWaylandCompositor/QWaylandTextInputManagerV3>
#include <QtWaylandCompositor/QWaylandQtTextInputMethodManager>
namespace LuDash {
void installInputMethodProtocols(QWaylandCompositor* compositor) {
    new QWaylandTextInputManager(compositor);
    new QWaylandTextInputManagerV3(compositor);
    new QWaylandQtTextInputMethodManager(compositor);
}
}
