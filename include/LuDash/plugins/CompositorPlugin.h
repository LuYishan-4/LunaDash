#pragma once
#include <QtPlugin>
class QQuickItem;
namespace LuDash {
class CompositorPlugin {
public:
    virtual ~CompositorPlugin() = default;
    virtual void windowOpened(QQuickItem* frame) = 0;
    virtual void windowFocused(QQuickItem* frame) = 0;
};
}
#define LUDASH_COMPOSITOR_PLUGIN_IID "org.ludash.CompositorPlugin/1.0"
Q_DECLARE_INTERFACE(LuDash::CompositorPlugin, LUDASH_COMPOSITOR_PLUGIN_IID)
