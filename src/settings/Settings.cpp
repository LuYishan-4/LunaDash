#include <LuDash/localization/Localization.h>
#include <QProcess>
#include <LuDash/settings/Settings.h>
#include <QtWidgets>
namespace LuDash {
QWidget* createSettings(bool tiled, const std::function<void(bool)>& setTiled) {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(28, 24, 28, 24); layout->setSpacing(18);
    auto* title = new QLabel(LuDash::translate("Make this space yours")); title->setObjectName("heading"); layout->addWidget(title);
    layout->addWidget(new QLabel(LuDash::translate("Interface language")));
    auto* language = new QComboBox; language->addItem(LuDash::translate("Traditional Chinese"), "zh_TW"); language->addItem("English", "en_US");
    language->setCurrentIndex(language->findData(selectedLanguage())); layout->addWidget(language);
    auto* languageHint = new QLabel(LuDash::translate("New applications use the selected language. Restart the compositor to update native plugins.")); languageHint->setWordWrap(true); layout->addWidget(languageHint);
    QObject::connect(language, &QComboBox::currentIndexChanged, page, [language] { QSettings().setValue("appearance/language", language->currentData()); });
    auto* inputHint = new QLabel(LuDash::translate("Input methods: Wayland text-input v2/v3 and Qt text-input. Fcitx5/IBus candidate windows and toolkit compatibility still require hardware testing.")); inputHint->setWordWrap(true); layout->addWidget(inputHint);
    auto* configureIme = new QPushButton(LuDash::translate("Configure input method")); layout->addWidget(configureIme);
    QObject::connect(configureIme, &QPushButton::clicked, page, [page] {
        for (const auto& name : {QString("fcitx5-configtool"), QString("ibus-setup")}) {
            const auto executable = QStandardPaths::findExecutable(name);
            if (!executable.isEmpty()) { QProcess::startDetached(executable, {}); return; }
        }
        QMessageBox::information(page, LuDash::translate("Input method settings unavailable"), LuDash::translate("Install fcitx5-configtool or ibus."));
    });
    layout->addWidget(new QLabel(LuDash::translate("Wallpaper colors")));
    auto* themes = new QComboBox; themes->addItems({LuDash::translate("Dusk mountains"), LuDash::translate("Forest mountains")}); themes->setCurrentIndex(QSettings().value("appearance/wallpaper", 0).toInt()); layout->addWidget(themes);
    QObject::connect(themes, &QComboBox::currentIndexChanged, page, [](int theme) { QSettings().setValue("appearance/wallpaper", theme); });
    auto* tiling = new QCheckBox(LuDash::translate("Tile application windows automatically")); tiling->setChecked(tiled); layout->addWidget(tiling); tiling->setVisible(bool(setTiled));
    if (setTiled) QObject::connect(tiling, &QCheckBox::toggled, page, setTiled);
    auto* shortcuts = new QLabel(LuDash::translate("Wayland shortcuts\nSuper+Enter  Console\nSuper+E  Files\nSuper+D  Applications\nSuper+J / K  Focus window\nSuper+1...4  Switch workspace\nSuper+Shift+1...4  Move window\nSuper+H / L  Adjust master ratio\nSuper+Space  Toggle floating\nSuper+Q  Close window"));
    shortcuts->setWordWrap(true); shortcuts->setObjectName("description"); layout->addWidget(shortcuts); layout->addStretch(); return page;
}
}
