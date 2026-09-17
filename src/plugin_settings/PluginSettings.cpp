#include <LuDash/localization/Localization.h>
#include <LuDash/plugin_settings/PluginSettings.h>
#include <LuDash/plugins/PluginManager.h>
#include <QtWidgets>

namespace LuDash {
QWidget *createPluginSettings() {
    auto *page = new QWidget;
    page->setWindowTitle(LuDash::translate("Plugins"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    auto *title = new QLabel(LuDash::translate("Plugins"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *tabs = new QTabWidget;
    layout->addWidget(tabs, 1);

    auto *installedPage = new QWidget;
    auto *installedLayout = new QVBoxLayout(installedPage);
    installedLayout->setContentsMargins(12, 16, 12, 12);
    installedLayout->setSpacing(12);

    auto *hint = new QLabel(LuDash::translate(
        "QML plugins are reloaded by the shell automatically. C++ effects run inside the compositor and require a session restart after changing their state."));
    hint->setWordWrap(true);
    installedLayout->addWidget(hint);

    const auto plugins = discoverPlugins();
    if (plugins.isEmpty()) {
        auto *empty = new QLabel(LuDash::translate(
            "No plugins found. QML plugins can be installed in the LunaDash shell plugins directory; native effects use the LuDash plugins directory."));
        empty->setWordWrap(true);
        installedLayout->addWidget(empty);
    }

    for (const auto &plugin : plugins) {
        auto *card = new QFrame;
        card->setFrameShape(QFrame::StyledPanel);
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(14);

        auto *icon = new QLabel;
        QIcon themed = QIcon::fromTheme(plugin.icon);
        if (!themed.isNull())
            icon->setPixmap(themed.pixmap(42, 42));
        else
            icon->setText(plugin.type == "qml" ? QStringLiteral("QML") : QStringLiteral("C++"));
        icon->setFixedSize(48, 48);
        icon->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(icon);

        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(3);
        auto *name = new QLabel(plugin.name.isEmpty() ? plugin.id : plugin.name);
        QFont nameFont = name->font();
        nameFont.setBold(true);
        name->setFont(nameFont);
        textLayout->addWidget(name);

        const QString typeLabel = plugin.type == "qml" ? LuDash::translate("QML")
                                                          : LuDash::translate("C++ effect");
        auto *meta = new QLabel(QStringLiteral("%1 · %2 · %3%4")
                                    .arg(typeLabel, plugin.version,
                                         plugin.author.isEmpty() ? LuDash::translate("Unknown author") : plugin.author,
                                         plugin.type == "effect" ? QStringLiteral(" · ") + LuDash::translate("Restart required") : QString()));
        meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
        textLayout->addWidget(meta);

        auto *description = new QLabel(plugin.error.isEmpty() ? plugin.description : plugin.error);
        description->setWordWrap(true);
        description->setStyleSheet(plugin.error.isEmpty() ? QString() : QStringLiteral("color:#d75f5f"));
        textLayout->addWidget(description);
        cardLayout->addLayout(textLayout, 1);

        auto *toggle = new QCheckBox(LuDash::translate("Enabled"));
        toggle->setChecked(plugin.enabled);
        toggle->setEnabled(plugin.error.isEmpty());
        cardLayout->addWidget(toggle);
        QObject::connect(toggle, &QCheckBox::toggled, page,
                         [=](bool enabled) {
            if (enabled && plugin.type == "effect" &&
                QMessageBox::question(
                    page, LuDash::translate("Enable C++ effect"),
                    LuDash::translate("This effect executes native code inside the compositor and may crash the desktop. Enable it?")) != QMessageBox::Yes) {
                QSignalBlocker blocker(toggle);
                toggle->setChecked(false);
                return;
            }
            QSettings settings;
            settings.setValue("plugins/" + plugin.id + "/enabled", enabled);
            settings.sync();
        });
        installedLayout->addWidget(card);
    }
    installedLayout->addStretch();

    auto *installedScroll = new QScrollArea;
    installedScroll->setWidgetResizable(true);
    installedScroll->setFrameShape(QFrame::NoFrame);
    installedScroll->setWidget(installedPage);
    tabs->addTab(installedScroll, LuDash::translate("Installed"));

    auto *storePage = new QWidget;
    auto *storeLayout = new QVBoxLayout(storePage);
    storeLayout->setContentsMargins(30, 30, 30, 30);
    auto *storeTitle = new QLabel(LuDash::translate("Plugin store is not supported yet"));
    QFont storeFont = storeTitle->font();
    storeFont.setPointSize(storeFont.pointSize() + 3);
    storeFont.setBold(true);
    storeTitle->setFont(storeFont);
    storeTitle->setAlignment(Qt::AlignCenter);
    storeLayout->addStretch();
    storeLayout->addWidget(storeTitle);
    auto *storeDescription = new QLabel(LuDash::translate(
        "The store page is reserved for a future plugin catalogue. Local QML plugins and C++ effects can already be discovered from their manifest.json files."));
    storeDescription->setWordWrap(true);
    storeDescription->setAlignment(Qt::AlignCenter);
    storeLayout->addWidget(storeDescription);
    storeLayout->addStretch();
    tabs->addTab(storePage, LuDash::translate("Store"));

    return page;
}
}
