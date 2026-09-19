#include "desktop/welcome/Welcome.hpp"
#include "config/localization/Localization.hpp"
#include <QtWidgets>

namespace LunaDash {
QWidget *createWelcome(const std::function<void(const QString &)> &launch) {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(32, 26, 32, 24);
  layout->setSpacing(18);
  auto *eyebrow = new QLabel("YOUR SPACE. YOUR PACE.");
  eyebrow->setObjectName("eyebrow");
  layout->addWidget(eyebrow);
  auto *title = new QLabel(LunaDash::translate("Welcome to LuDash."));
  title->setObjectName("heroTitle");
  layout->addWidget(title);
  auto *description = new QLabel(
      LunaDash::translate("A quiet, focused Linux desktop.\nOpen your files, "
                          "make yourself at home, and start your day."));
  description->setObjectName("description");
  layout->addWidget(description);
  auto *row = new QHBoxLayout;
  for (const auto &pair :
       {qMakePair(QString("files"),
                  QString(LunaDash::translate("Browse files"))),
        qMakePair(QString("settings"),
                  QString(LunaDash::translate("Desktop settings")))}) {
    auto *button = new QPushButton(pair.second);
    button->setMinimumHeight(46);
    row->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, page,
                     [launch, id = pair.first] { launch(id); });
  }
  layout->addLayout(row);
  layout->addStretch();
  auto *footer = new QLabel(
      "C++20  /  OPENGL                          LUDASH DESKTOP  0.1");
  footer->setObjectName("muted");
  layout->addWidget(footer);
  return page;
}
} // namespace LunaDash
