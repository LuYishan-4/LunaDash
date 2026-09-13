#include <LuDash/localization/Localization.h>
#include <LuDash/plugin_settings/PluginSettings.h>
#include <LuDash/plugins/PluginManager.h>
#include <QtWidgets>
namespace LuDash {
QWidget* createPluginSettings() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page);
    auto* hint = new QLabel(LuDash::translate("Native plugins run inside the compositor. Enable only trusted plugins. Changes apply after restarting the desktop.")); hint->setWordWrap(true); layout->addWidget(hint);
    const auto plugins = discoverPlugins();
    if (plugins.isEmpty()) layout->addWidget(new QLabel(LuDash::translate("No plugins found. Place metadata.json and the library in ~/.local/share/ludash/plugins/<id>/.")));
    for (const auto& plugin : plugins) {
        auto* checkbox = new QCheckBox(plugin.name.isEmpty() ? plugin.error : plugin.name + "  " + plugin.version);
        checkbox->setChecked(plugin.enabled); checkbox->setEnabled(plugin.error.isEmpty());
        checkbox->setToolTip(plugin.error.isEmpty() ? plugin.description : plugin.error); layout->addWidget(checkbox);
        QObject::connect(checkbox, &QCheckBox::toggled, page, [=](bool enabled) {
            if (enabled && QMessageBox::question(page, LuDash::translate("Enable native plugin"), LuDash::translate("This plugin can execute native code and may crash the desktop. Enable it?")) != QMessageBox::Yes) {
                QSignalBlocker blocker(checkbox); checkbox->setChecked(false); return;
            }
            QSettings().setValue("plugins/" + plugin.id + "/enabled", enabled);
        });
    }
    layout->addStretch(); return page;
}
}
