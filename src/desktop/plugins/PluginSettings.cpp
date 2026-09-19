#include "desktop/plugins/PluginSettings.hpp"
#include "config/localization/Localization.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include <QtWidgets>

namespace LunaDash {
QWidget *createPluginSettings() {
  auto *page = new QWidget;
  page->setWindowTitle(LunaDash::translate("Plugins"));
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(22, 22, 22, 22);
  layout->setSpacing(14);

  auto *title = new QLabel(LunaDash::translate("Plugins"));
  QFont titleFont = title->font();
  titleFont.setPointSize(titleFont.pointSize() + 6);
  titleFont.setBold(true);
  title->setFont(titleFont);
  title->setObjectName("heading");
  layout->addWidget(title);

  auto *tabs = new QTabWidget;
  layout->addWidget(tabs, 1);

  auto *installedPage = new QWidget;
  installedPage->setObjectName("pluginPage");
  auto *installedLayout = new QVBoxLayout(installedPage);
  installedLayout->setContentsMargins(12, 16, 12, 12);
  installedLayout->setSpacing(12);

  auto *hint = new QLabel(LunaDash::translate(
      "QML plugins are reloaded by the shell automatically. C++ effects run "
      "inside the compositor and require a session restart after changing "
      "their state."));
  hint->setWordWrap(true);
  hint->setObjectName("description");
  installedLayout->addWidget(hint);

  const auto plugins = discoverPlugins();
  if (plugins.isEmpty()) {
    auto *empty = new QLabel(LunaDash::translate(
        "No plugins found. QML plugins can be installed in the LunaDash shell "
        "plugins directory; native effects use the LuDash plugins directory."));
    empty->setWordWrap(true);
    installedLayout->addWidget(empty);
  }

  for (const auto &plugin : plugins) {
    auto *card = new QFrame;
    card->setObjectName("pluginCard");
    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(14);

    auto *icon = new QLabel;
    icon->setObjectName("pluginIcon");
    QIcon themed = QIcon::fromTheme(plugin.icon);
    if (!themed.isNull())
      icon->setPixmap(themed.pixmap(42, 42));
    else
      icon->setText(plugin.type == "qml" ? QStringLiteral("QML")
                                         : QStringLiteral("C++"));
    icon->setFixedSize(48, 48);
    icon->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(icon);

    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(3);
    auto *name = new QLabel(plugin.name.isEmpty() ? plugin.id : plugin.name);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    name->setFont(nameFont);
    name->setObjectName("pluginName");
    name->setWordWrap(true);
    textLayout->addWidget(name);

    const QString typeLabel = plugin.type == "qml"
                                  ? LunaDash::translate("QML")
                                  : LunaDash::translate("C++ effect");
    auto *meta = new QLabel(
        QStringLiteral("%1 · %2 · %3%4")
            .arg(typeLabel, plugin.version,
                 plugin.author.isEmpty() ? LunaDash::translate("Unknown author")
                                         : plugin.author,
                 plugin.type == "effect"
                     ? QStringLiteral(" · ") +
                           LunaDash::translate("Restart required")
                     : QString()));
    meta->setObjectName("muted");
    meta->setWordWrap(true);
    meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLayout->addWidget(meta);

    auto *description =
        new QLabel(plugin.error.isEmpty() ? plugin.description : plugin.error);
    description->setWordWrap(true);
    description->setObjectName(plugin.error.isEmpty() ? "description"
                                                      : "danger");
    textLayout->addWidget(description);
    cardLayout->addLayout(textLayout, 1);

    auto *toggle = new QCheckBox(LunaDash::translate("Enabled"));
    toggle->setChecked(plugin.enabled);
    toggle->setEnabled(plugin.error.isEmpty());
    cardLayout->addWidget(toggle);
    QObject::connect(toggle, &QCheckBox::toggled, page, [=](bool enabled) {
      if (enabled && plugin.type == "effect" &&
          QMessageBox::question(
              page, LunaDash::translate("Enable C++ effect"),
              LunaDash::translate(
                  "This effect executes native code inside the compositor and "
                  "may crash the desktop. Enable it?")) != QMessageBox::Yes) {
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
  tabs->addTab(installedScroll, LunaDash::translate("Installed"));

  auto *storePage = new QWidget;
  storePage->setObjectName("pluginStore");
  auto *storeLayout = new QVBoxLayout(storePage);
  storeLayout->setContentsMargins(30, 30, 30, 30);
  auto *storeTitle =
      new QLabel(LunaDash::translate("Plugin store is not supported yet"));
  QFont storeFont = storeTitle->font();
  storeFont.setPointSize(storeFont.pointSize() + 3);
  storeFont.setBold(true);
  storeTitle->setFont(storeFont);
  storeTitle->setWordWrap(true);
  storeTitle->setAlignment(Qt::AlignCenter);
  storeLayout->addStretch();
  storeLayout->addWidget(storeTitle);
  auto *storeDescription = new QLabel(LunaDash::translate(
      "The store page is reserved for a future plugin catalogue. Local QML "
      "plugins and C++ effects can already be discovered from their "
      "manifest.json files."));
  storeDescription->setWordWrap(true);
  storeDescription->setObjectName("description");
  storeDescription->setAlignment(Qt::AlignCenter);
  storeLayout->addWidget(storeDescription);
  storeLayout->addStretch();
  tabs->addTab(storePage, LunaDash::translate("Store"));

  return page;
}
} // namespace LunaDash
