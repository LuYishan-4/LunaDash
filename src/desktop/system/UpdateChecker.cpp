#include "desktop/system/UpdateChecker.hpp"
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
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
namespace {
constexpr auto kReleaseApi =
    "https://api.github.com/repos/LuYishan-4/LunaDash/releases/latest";
constexpr auto kDevCommitApi =
    "https://api.github.com/repos/LuYishan-4/LunaDash/commits/dev";
constexpr auto kRepositoryUrl = "https://github.com/LuYishan-4/LunaDash";
constexpr qsizetype kMaximumReplyBytes = 256 * 1024;

struct ParsedVersion {
  QList<int> core;
  QStringList prerelease;
  bool valid = false;
};

// Parse a tag such as "v1.2.0", "1.2" or "1.2.0-rc1". The pre-release suffix is
// kept so "1.2.0-rc1" is not treated as the final "1.2.0".
ParsedVersion parseVersion(QString version) {
  if (version.startsWith('v'))
    version.remove(0, 1);
  ParsedVersion result;
  int split = version.indexOf('-');
  if (split < 0) {
    for (qsizetype i = 0; i < version.size(); ++i) {
      const QChar ch = version.at(i);
      if (!ch.isDigit() && ch != '.') {
        split = static_cast<int>(i);
        break;
      }
    }
  }
  const QString core = split >= 0 ? version.left(split) : version;
  QString prerelease = split >= 0 ? version.mid(split) : QString();
  if (prerelease.startsWith('-'))
    prerelease.remove(0, 1);
  if (core.isEmpty())
    return result;
  for (const auto &part : core.split('.')) {
    bool ok = false;
    const int number = part.toInt(&ok);
    if (!ok || number < 0)
      return result;
    result.core.append(number);
  }
  result.valid = true;
  if (!prerelease.isEmpty())
    result.prerelease = prerelease.split('.', Qt::SkipEmptyParts);
  return result;
}

bool isNewer(const QString &latest, const QString &current) {
  const auto left = parseVersion(latest);
  const auto right = parseVersion(current);
  if (!left.valid || !right.valid)
    return false;
  auto leftCore = left.core;
  auto rightCore = right.core;
  while (leftCore.size() < rightCore.size())
    leftCore.append(0);
  while (rightCore.size() < leftCore.size())
    rightCore.append(0);
  for (qsizetype i = 0; i < leftCore.size(); ++i)
    if (leftCore[i] != rightCore[i])
      return leftCore[i] > rightCore[i];
  // Equal numeric core: a release sorts after any pre-release of that core.
  const auto &leftPre = left.prerelease;
  const auto &rightPre = right.prerelease;
  if (leftPre.isEmpty() || rightPre.isEmpty())
    return leftPre.isEmpty() && !rightPre.isEmpty();
  const auto count = std::min(leftPre.size(), rightPre.size());
  for (qsizetype i = 0; i < count; ++i) {
    bool leftNumeric = false;
    bool rightNumeric = false;
    const int leftNumber = leftPre[i].toInt(&leftNumeric);
    const int rightNumber = rightPre[i].toInt(&rightNumeric);
    if (leftNumeric && rightNumeric) {
      if (leftNumber != rightNumber)
        return leftNumber > rightNumber;
    } else if (leftNumeric != rightNumeric) {
      return rightNumeric; // numeric identifiers sort before alphanumeric ones
    } else if (leftPre[i] != rightPre[i]) {
      return leftPre[i] > rightPre[i];
    }
  }
  return leftPre.size() > rightPre.size();
}

QString selectedChannel() {
  const auto value =
      QSettings().value("desktop/updateChannel", "stable").toString();
  return value == "dev" ? QStringLiteral("dev") : QStringLiteral("stable");
}

QString detectCurrentCommit() {
  QString commit = QStringLiteral(LUDASH_GIT_COMMIT);
  if (!commit.isEmpty() && commit != "unknown")
    return commit;
  QProcess git;
  git.start("git", {"rev-parse", "--verify", "HEAD"});
  if (git.waitForFinished(500) && git.exitStatus() == QProcess::NormalExit &&
      git.exitCode() == 0) {
    commit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    if (commit.size() >= 12)
      return commit;
  }
  return QStringLiteral("unknown");
}

QString updateStateDirectory() {
  QString root = qEnvironmentVariable("XDG_STATE_HOME").trimmed();
  if (root.isEmpty())
    root = QDir::homePath() + QStringLiteral("/.local/state");
  return QDir(root).filePath(QStringLiteral("lunadash/update"));
}

bool updaterProcessAlive(qint64 pid) {
  if (pid <= 1)
    return false;
  QFile commandLine(QStringLiteral("/proc/%1/cmdline").arg(pid));
  if (!commandLine.open(QIODevice::ReadOnly))
    return false;
  const QByteArray command = commandLine.read(4096);
  return command.contains("lunadash-update");
}

QJsonObject installSnapshot() {
  const QString directory = updateStateDirectory();
  const QString path =
      QDir(directory).filePath(QStringLiteral("progress.json"));
  QFile file(path);
  QJsonObject install{
      {"state", "idle"},   {"progress", 0},
      {"stage", "idle"},   {"message", ""},
      {"channel", ""},     {"target", ""},
      {"rollback", false}, {"available", QFileInfo::exists(path)}};

  if (file.open(QIODevice::ReadOnly)) {
    const QByteArray bytes = file.read(32 * 1024 + 1);
    if (bytes.size() <= 32 * 1024) {
      QJsonParseError error;
      const auto document = QJsonDocument::fromJson(bytes, &error);
      if (error.error == QJsonParseError::NoError && document.isObject()) {
        const auto object = document.object();
        QString state = object.value("state").toString();
        QString stage = object.value("stage").toString();
        QString message = object.value("message").toString().left(1024);
        const qint64 pid = static_cast<qint64>(object.value("pid").toDouble(0));
        const qint64 updatedAt =
            static_cast<qint64>(object.value("updatedAt").toDouble(0));

        // progress.json survives shell/compositor restarts. A previous updater
        // that was killed while building used to leave state="running" forever,
        // which made About animate indefinitely at the last percentage. Running
        // is valid only while the recorded lunadash-update process still
        // exists.
        if (state == "running" && !updaterProcessAlive(pid)) {
          state = "error";
          stage = "interrupted";
          message =
              "The previous update was interrupted and is no longer running.";
        }

        if (QStringList{"idle", "running", "completed", "error"}.contains(
                state))
          install["state"] = state;
        if (!stage.isEmpty() && stage.size() <= 64)
          install["stage"] = stage;
        install["progress"] =
            std::clamp(object.value("progress").toInt(), 0, 100);
        install["message"] = message;
        install["channel"] = object.value("channel").toString().left(16);
        install["target"] = object.value("target").toString().left(128);
        install["rollback"] = object.value("rollback").toBool();
        install["pid"] = pid;
        install["updatedAt"] = updatedAt;
        install["active"] = state == "running";
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
  startedAt_ = QDateTime::currentSecsSinceEpoch();
  timeout_->setSingleShot(true);
  timeout_->setInterval(8000);
  connect(timeout_, &QTimer::timeout, this, [this] {
    if (reply_)
      reply_->abort();
    finishWithError("The update check timed out.");
  });
}

QJsonObject UpdateChecker::snapshot() const {
  return {{"status", status_},
          {"channel", channel_},
          {"currentVersion", QStringLiteral(LUDASH_VERSION)},
          {"currentCommit", currentCommit_},
          {"startedAt", startedAt_},
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
  if (channel != "stable" && channel != "dev")
    return false;
  if (channel == channel_)
    return true;
  timeout_->stop();
  if (reply_) {
    auto *previous = reply_;
    reply_ = nullptr;
    previous->disconnect(this);
    previous->abort();
    previous->deleteLater();
  }
  channel_ = channel;
  QSettings().setValue("desktop/updateChannel", channel_);
  status_ = "idle";
  latestVersion_.clear();
  latestCommit_.clear();
  latestMessage_.clear();
  releaseUrl_.clear();
  error_.clear();
  checkedAt_.clear();
  emit changed();
  return true;
}

void UpdateChecker::check() {
  setChannel(selectedChannel());
  if (reply_)
    return;
  status_ = "checking";
  latestVersion_.clear();
  latestCommit_.clear();
  latestMessage_.clear();
  releaseUrl_.clear();
  error_.clear();
  emit changed();
  QNetworkRequest request{QUrl(
      QString::fromLatin1(channel_ == "dev" ? kDevCommitApi : kReleaseApi))};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("LunaDash/%1").arg(LUDASH_VERSION));
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  reply_ = network_->get(request);
  connect(reply_, &QNetworkReply::finished, this, &UpdateChecker::finishReply);
  connect(reply_, &QIODevice::readyRead, this, [this] {
    if (reply_ && reply_->bytesAvailable() > kMaximumReplyBytes)
      reply_->abort();
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
  if (!reply_)
    return;
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
    latestMessage_ =
        object.value("commit").toObject().value("message").toString().section(
            '\n', 0, 0);
    releaseUrl_ = object.value("html_url").toString();
    const QUrl url(releaseUrl_);
    if (latestCommit_.size() < 12 || url.scheme() != "https" ||
        url.host() != "github.com" ||
        !url.path().startsWith("/LuYishan-4/LunaDash/commit/")) {
      finishWithError("GitHub returned unexpected commit information.");
      return;
    }
    status_ =
        currentCommit_ != "unknown" && latestCommit_.startsWith(currentCommit_)
            ? "upToDate"
            : "available";
  } else {
    latestVersion_ = object.value("tag_name").toString();
    releaseUrl_ = object.value("html_url").toString();
    const QUrl url(releaseUrl_);
    if (!parseVersion(latestVersion_).valid || url.scheme() != "https" ||
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

} // namespace LunaDash
