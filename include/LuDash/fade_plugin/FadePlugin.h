#pragma once
#include <LuDash/plugins/CompositorPlugin.h>
#include <QObject>
namespace LuDash {
class FadePlugin final : public QObject, public CompositorPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LUDASH_COMPOSITOR_PLUGIN_IID FILE "../../../data/plugins/fade/metadata.json")
    Q_INTERFACES(LuDash::CompositorPlugin)
public:
    void windowOpened(QQuickItem* frame) override;
    void windowFocused(QQuickItem* frame) override;
};
}
