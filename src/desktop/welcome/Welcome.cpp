#include "desktop/welcome/Welcome.hpp"
#include "config/BuildConfig.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QQmlContext>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QTimer>

namespace LunaDash {
WelcomeBridge::WelcomeBridge(QObject *parent) : QObject(parent) {
  const auto preferences = desktopPreferences();
  const auto language = selectedLanguage();
  state_ = {{"version", BuildConfig::version}, {"language", language},
            {"translations", languageDictionary(language).toVariantMap()},
            {"appearance", preferences.toVariantMap()},
            {"palette", appearancePalette(preferences).toVariantMap()},
            {"shortcuts", ShortcutSettings().snapshot().toVariantMap()}};
  QTimer::singleShot(0, this, [this] { command("status", {}); });
}

QVariantMap WelcomeBridge::state() const { return state_; }

void WelcomeBridge::command(const QString &method, const QString &value) {
  if (commands_.size() >= 16)
    return;
  commands_.enqueue({method, value});
  dispatch();
}

void WelcomeBridge::reportError() {
  state_["welcomeError"] = translate("Could not contact the desktop.");
  emit stateChanged();
}

void WelcomeBridge::dispatch() {
  if (pending_ || commands_.isEmpty())
    return;
  const auto [method, value] = commands_.dequeue();
  pending_ = true;
  auto *process = new QProcess(this);
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, process, [process] { process->kill(); });
  connect(process, &QProcess::errorOccurred, this,
          [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              pending_ = false;
              reportError();
              process->deleteLater();
              dispatch();
            }
          });
  connect(process, &QProcess::finished, this,
          [this, process](int code, QProcess::ExitStatus status) {
            pending_ = false;
            const auto document = QJsonDocument::fromJson(process->readAllStandardOutput());
            if (code == 0 && status == QProcess::NormalExit && document.isObject() &&
                !document.object().contains("error")) {
              state_ = document.object().toVariantMap();
              emit stateChanged();
            } else {
              reportError();
            }
            process->deleteLater();
            dispatch();
          });
  process->start(QCoreApplication::applicationDirPath() + "/lunadashctl", {method, value});
  timeout->start(5000);
}

void WelcomeBridge::finish() { emit finished(); }

QWidget *createWelcome() {
  auto *page = new QQuickWidget;
  page->setObjectName("welcomePage");
  page->setResizeMode(QQuickWidget::SizeRootObjectToView);
  page->setClearColor(Qt::transparent);
  auto *bridge = new WelcomeBridge(page);
  page->rootContext()->setContextProperty("welcomeBridge", bridge);
  QObject::connect(bridge, &WelcomeBridge::finished, page,
                   [page] { page->window()->close(); });
  const QString relative = "welcome/WelcomeStandalone.qml";
  auto path = QDir(QCoreApplication::applicationDirPath())
                  .absoluteFilePath("../share/lunadash/shell/" + relative);
  if (!QFileInfo::exists(path))
    path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  "lunadash/shell/" + relative);
  if (path.isEmpty())
    path = QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/" + relative;
  page->setSource(QUrl::fromLocalFile(path));
  if (page->status() == QQuickWidget::Error) {
    qWarning("Welcome QML could not be loaded.");
    delete page;
    return nullptr;
  }
  return page;
}
} // namespace LunaDash
