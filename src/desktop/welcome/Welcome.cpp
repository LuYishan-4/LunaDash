#include "desktop/welcome/Welcome.hpp"
#include "config/BuildConfig.hpp"
#include "config/localization/Localization.hpp"
#include <QSysInfo>
#include <QtWidgets>

namespace LunaDash {
namespace {
QFrame *infoCard(const QString &eyebrowText, const QString &value,
                 const QString &detail) {
  auto *card = new QFrame;
  card->setObjectName("welcomeCard");
  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(5);

  auto *eyebrow = new QLabel(eyebrowText);
  eyebrow->setObjectName("welcomeEyebrow");
  layout->addWidget(eyebrow);

  auto *title = new QLabel(value);
  title->setObjectName("welcomeValue");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto *description = new QLabel(detail);
  description->setObjectName("welcomeDetail");
  description->setWordWrap(true);
  layout->addWidget(description);
  layout->addStretch();
  return card;
}
} // namespace

QWidget *createWelcome(const std::function<void(const QString &)> &launch) {
  auto *page = new QWidget;
  page->setObjectName("welcomePage");

  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(28, 24, 28, 24);
  root->setSpacing(16);

  auto *hero = new QFrame;
  hero->setObjectName("welcomeHero");
  auto *heroLayout = new QHBoxLayout(hero);
  heroLayout->setContentsMargins(24, 22, 24, 22);
  heroLayout->setSpacing(20);

  auto *copy = new QVBoxLayout;
  copy->setSpacing(6);
  auto *eyebrow = new QLabel("LUNADASH  /  WAYLAND DESKTOP");
  eyebrow->setObjectName("welcomeEyebrow");
  copy->addWidget(eyebrow);

  auto *title = new QLabel(LunaDash::translate("Welcome to LunaDash"));
  title->setObjectName("welcomeHeroTitle");
  copy->addWidget(title);

  auto *description = new QLabel(LunaDash::translate(
      "Your desktop is ready. Open a workspace, tune the system, or continue "
      "where you left off."));
  description->setObjectName("welcomeHeroDescription");
  description->setWordWrap(true);
  copy->addWidget(description);

  auto *version = new QLabel(QStringLiteral("VERSION %1  ·  %2")
                                 .arg(QString::fromLatin1(BuildConfig::version),
                                      QSysInfo::prettyProductName()));
  version->setObjectName("welcomeMeta");
  copy->addWidget(version);
  copy->addStretch();

  auto *actions = new QGridLayout;
  actions->setHorizontalSpacing(8);
  actions->setVerticalSpacing(8);
  const QList<QPair<QString, QString>> entries{
      {"files", LunaDash::translate("Browse files")},
      {"settings", LunaDash::translate("Desktop settings")},
      {"plugins", LunaDash::translate("Plugins")},
      {"packages", LunaDash::translate("Packages")}};
  int index = 0;
  for (const auto &entry : entries) {
    auto *button = new QPushButton(entry.second);
    button->setObjectName(index == 0 ? "accent" : "welcomeAction");
    button->setMinimumHeight(42);
    actions->addWidget(button, index / 2, index % 2);
    QObject::connect(button, &QPushButton::clicked, page,
                     [launch, id = entry.first] { launch(id); });
    ++index;
  }
  copy->addLayout(actions);
  heroLayout->addLayout(copy, 1);

  auto *mark = new QFrame;
  mark->setObjectName("welcomeMark");
  mark->setFixedSize(164, 164);
  auto *markLayout = new QVBoxLayout(mark);
  markLayout->setContentsMargins(16, 16, 16, 16);
  auto *glyph = new QLabel(QStringLiteral("◈"));
  glyph->setObjectName("welcomeGlyph");
  glyph->setAlignment(Qt::AlignCenter);
  markLayout->addWidget(glyph);
  auto *markText = new QLabel(QStringLiteral("1.0.1a"));
  markText->setObjectName("welcomeMeta");
  markText->setAlignment(Qt::AlignCenter);
  markLayout->addWidget(markText);
  heroLayout->addWidget(mark, 0, Qt::AlignCenter);

  root->addWidget(hero);

  auto *cards = new QHBoxLayout;
  cards->setSpacing(12);
  cards->addWidget(infoCard(
      LunaDash::translate("Desktop"),
      LunaDash::translate("Wayland session"),
      LunaDash::translate("wlroots compositor, rootless XWayland and native input.")));
  cards->addWidget(infoCard(
      LunaDash::translate("Customize"),
      LunaDash::translate("Modules and plugins"),
      LunaDash::translate("Panel, wallpaper, dashboard and desktop widgets can be changed live.")));
  cards->addWidget(infoCard(
      LunaDash::translate("System"),
      QSysInfo::kernelType() + " " + QSysInfo::kernelVersion(),
      QSysInfo::currentCpuArchitecture()));
  root->addLayout(cards);

  auto *footer = new QHBoxLayout;
  auto *hint = new QLabel(LunaDash::translate(
      "Tip: use Settings for desktop controls and the launcher for installed applications."));
  hint->setObjectName("welcomeDetail");
  hint->setWordWrap(true);
  footer->addWidget(hint, 1);
  auto *status = new QLabel(QStringLiteral("LUNADASH  %1")
                                .arg(QString::fromLatin1(BuildConfig::version)));
  status->setObjectName("welcomeMeta");
  footer->addWidget(status);
  root->addLayout(footer);
  return page;
}
} // namespace LunaDash
