#include "compositor/UpdateChecker/UpdateChecker.hpp"
#include "config/BuildConfig.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#ifndef LUNADASH_GIT_COMMIT
#define LUNADASH_GIT_COMMIT "unknown"
#endif

namespace LuDash {
namespace {
constexpr auto kReleaseApi = "https://api.github.com/repos/LuYishan-4/LunaDash/releases/latest";
constexpr auto kDevCommitApi = "https://api.github.com/repos/LuYishan-4/LunaDash/commits/dev";
constexpr auto kRepositoryUrl = "https://github.com/LuYishan-4/LunaDash";
constexpr qsizetype kMaximumReplyBytes = 256 * 1024;

QList<int> versionParts(QString version) {
  if (version.startsWith('v')) version.remove(0, 1);
  QList<int> parts;
  for (const auto &part : version.split('.')) {
    bool ok = false;
    const int number = part.section('-', 0, 0).toInt(&ok);
    if (!ok || number < 0) return {};
    parts.append(number);
  }
  return parts;
}

bool isNewer(const QString &latest, const QString &current) {
  auto left = versionParts(latest);
  auto right = versionParts(current);
  if (left.isEmpty() || right.isEmpty()) return false;
  while (left.size() < right.size()) left.append(0);
  while (right.size() < left.size()) right.append(0);
  for (qsizetype i = 0; i < left.size(); ++i)
    if (left[i] != right[i]) return left[i] > right[i];
  return false;
}

QString selectedChannel() {
  const auto value = QSettings().value("desktop/updateChannel", "stable").toString();
  return value == "dev" ? QStringLiteral("dev") : QStringLiteral("stable");
}

QString detectCurrentCommit() {
  QString commit = QStringLiteral(LUNADASH_GIT_COMMIT);
  if (!commit.isEmpty() && commit != "unknown") return commit;
  QProcess git;
  git.start("git", {"rev-parse", "--verify", "HEAD"});
  if (git.waitForFinished(500) && git.exitStatus() == QProcess::NormalExit &&
      git.exitCode() == 0) {
    commit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    if (commit.size() >= 12) return commit;
  }
  return QStringLiteral("unknown");
}

QString updateStateDirectory() {
  QString root = qEnvironmentVariable("XDG_STATE_HOME").trimmed();
  if (root.isEmpty())
    root = QDir::homePath() + QStringLiteral("/.local/state");
  return QDir(root).filePath(QStringLiteral("lunadash/update"));
}

QJsonObject installSnapshot() {
  const QString directory = updateStateDirectory();
  const QString path = QDir(directory).filePath(QStringLiteral("progress.json"));
  QFile file(path);
  QJsonObject install{{"state", "idle"},
                      {"progress", 0},
                      {"stage", "idle"},
                      {"message", ""},
                      {"channel", ""},
                      {"target", ""},
                      {"rollback", false},
                      {"available", QFileInfo::exists(path)}};

  if (file.open(QIODevice::ReadOnly)) {
    const QByteArray bytes = file.read(32 * 1024 + 1);
    if (bytes.size() <= 32 * 1024) {
      QJsonParseError error;
      const auto document = QJsonDocument::fromJson(bytes, &error);
      if (error.error == QJsonParseError::NoError && document.isObject()) {
        const auto object = document.object();
        const QString state = object.value("state").toString();
        const QString stage = object.value("stage").toString();
        if (QStringList{"idle", "running", "completed", "error"}.contains(state))
          install["state"] = state;
        if (!stage.isEmpty() && stage.size() <= 64)
          install["stage"] = stage;
        install["progress"] =
            std::clamp(object.value("progress").toInt(), 0, 100);
        install["message"] = object.value("message").toString().left(1024);
        install["channel"] = object.value("channel").toString().left(16);
        install["target"] = object.value("target").toString().left(128);
        install["rollback"] = object.value("rollback").toBool();
        install["available"] = true;
      }
    }
  }

  QFile last(QDir(directory).filePath(QStringLiteral("last-update")));
  if (last.open(QIODevice::ReadOnly))
    install["lastUpdate"] =
        QString::fromUtf8(last.read(1024)).trimmed().left(256);
  return install;
}
} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)),
      timeout_(new QTimer(this)), channel_(selectedChannel()),
      currentCommit_(detectCurrentCommit()) {
  timeout_->setSingleShot(true);
  timeout_->setInterval(8000);
  connect(timeout_, &QTimer::timeout, this, [this] {
    if (reply_) reply_->abort();
    finishWithError("The update check timed out.");
  });
}

QJsonObject UpdateChecker::snapshot() const {
  return {{"status", status_},
          {"channel", channel_},
          {"currentVersion", QStringLiteral(LUDASH_VERSION)},
          {"currentCommit", currentCommit_},
          {"latestVersion", latestVersion_},
          {"latestCommit", latestCommit_},
          {"latestMessage", latestMessage_},
          {"releaseUrl", releaseUrl_},
          {"repositoryUrl", QString::fromLatin1(kRepositoryUrl)},
          {"error", error_},
          {"checkedAt", checkedAt_},
          {"install", installSnapshot()}};
}

bool UpdateChecker::setChannel(const QString &channel) {
  if (channel != "stable" && channel != "dev") return false;
  channel_ = channel;
  QSettings().setValue("desktop/updateChannel", channel_);
  status_ = "idle";
  latestVersion_.clear();
  latestCommit_.clear();
  latestMessage_.clear();
  releaseUrl_.clear();
  error_.clear();
  emit changed();
  return true;
}

void UpdateChecker::check() {
  if (reply_) return;
  channel_ = selectedChannel();
  status_ = "checking";
  latestVersion_.clear();
  latestCommit_.clear();
  latestMessage_.clear();
  releaseUrl_.clear();
  error_.clear();
  emit changed();
  QNetworkRequest request{QUrl(QString::fromLatin1(channel_ == "dev" ? kDevCommitApi : kReleaseApi))};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("LunaDash/%1").arg(LUDASH_VERSION));
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  reply_ = network_->get(request);
  connect(reply_, &QNetworkReply::finished, this, &UpdateChecker::finishReply);
  connect(reply_, &QIODevice::readyRead, this, [this] {
    if (reply_ && reply_->bytesAvailable() > kMaximumReplyBytes) reply_->abort();
  });
  timeout_->start();
}

void UpdateChecker::finishWithError(const QString &message) {
  timeout_->stop();
  if (reply_) {
    reply_->deleteLater();
    reply_ = nullptr;
  }
  status_ = "error";
  error_ = message;
  checkedAt_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  emit changed();
}

void UpdateChecker::finishReply() {
  if (!reply_) return;
  timeout_->stop();
  const auto networkError = reply_->error();
  const QByteArray body = reply_->readAll();
  const auto finalUrl = reply_->url();
  reply_->deleteLater();
  reply_ = nullptr;
  if (networkError != QNetworkReply::NoError) {
    finishWithError("GitHub could not be reached.");
    return;
  }
  if (body.size() > kMaximumReplyBytes || finalUrl.scheme() != "https" ||
      finalUrl.host() != "api.github.com") {
    finishWithError("The update response was rejected.");
    return;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(body, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    finishWithError("GitHub returned invalid update information.");
    return;
  }
  const auto object = document.object();
  if (channel_ == "dev") {
    latestCommit_ = object.value("sha").toString();
    latestMessage_ = object.value("commit").toObject().value("message").toString().section('\n', 0, 0);
    releaseUrl_ = object.value("html_url").toString();
    const QUrl url(releaseUrl_);
    if (latestCommit_.size() < 12 || url.scheme() != "https" ||
        url.host() != "github.com" ||
        !url.path().startsWith("/LuYishan-4/LunaDash/commit/")) {
      finishWithError("GitHub returned unexpected commit information.");
      return;
    }
    status_ = currentCommit_ != "unknown" && latestCommit_.startsWith(currentCommit_)
                  ? "upToDate"
                  : "available";
  } else {
    latestVersion_ = object.value("tag_name").toString();
    releaseUrl_ = object.value("html_url").toString();
    const QUrl url(releaseUrl_);
    if (versionParts(latestVersion_).isEmpty() || url.scheme() != "https" ||
        url.host() != "github.com" ||
        !url.path().startsWith("/LuYishan-4/LunaDash/releases/")) {
      finishWithError("GitHub returned unexpected release information.");
      return;
    }
    status_ = isNewer(latestVersion_, QStringLiteral(LUDASH_VERSION))
                  ? "available"
                  : "upToDate";
  }
  error_.clear();
  checkedAt_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  emit changed();
}

} // namespace LuDash
