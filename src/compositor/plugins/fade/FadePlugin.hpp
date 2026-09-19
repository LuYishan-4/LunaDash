#pragma once
#include "compositor/plugins/CompositorPlugin.hpp"
#include <QObject>
namespace LunaDash {
class FadePlugin final : public QObject, public CompositorPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID LUDASH_COMPOSITOR_PLUGIN_IID FILE
                    "../../../../data/plugins/fade/metadata.json")
  Q_INTERFACES(LunaDash::CompositorPlugin)
public:
  void windowOpened(QQuickItem *frame) override;
  void windowFocused(QQuickItem *frame) override;
};
} // namespace LunaDash
