#include <LuDash/update_check/UpdateChecker.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace LuDash {
namespace {
constexpr auto kReleaseApi =
    "https://api.github.com/repos/LuYishan-4/LuDash/releases/latest";
constexpr auto kRepositoryUrl = "https://github.com/LuYishan-4/LuDash";
constexpr qsizetype kMaximumReplyBytes = 256 * 1024;

QList<int> versionParts(QString version) {
  if (version.startsWith('v'))
    version.remove(0, 1);
  QList<int> parts;
  for (const auto &part : version.split('.')) {
    bool ok = false;
    const int number = part.section('-', 0, 0).toInt(&ok);
    if (!ok || number < 0)
      return {};
    parts.append(number);
  }
  return parts;
}

bool isNewer(const QString &latest, const QString &current) {
  auto left = versionParts(latest);
  auto right = versionParts(current);
  if (left.isEmpty() || right.isEmpty())
    return false;
  while (left.size() < right.size())
    left.append(0);
  while (right.size() < left.size())
    right.append(0);
  for (qsizetype index = 0; index < left.size(); ++index) {
    if (left[index] != right[index])
      return left[index] > right[index];
  }
  return false;
}
} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)),
      timeout_(new QTimer(this)) {
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
          {"currentVersion", QStringLiteral(LUNADAH_VERSION)},
          {"latestVersion", latestVersion_},
          {"releaseUrl", releaseUrl_},
          {"repositoryUrl", QString::fromLatin1(kRepositoryUrl)},
          {"error", error_},
          {"checkedAt", checkedAt_}};
}

void UpdateChecker::check() {
  if (reply_)
    return;
  status_ = "checking";
  latestVersion_.clear();
  releaseUrl_.clear();
  error_.clear();
  emit changed();

  QNetworkRequest request{QUrl(QString::fromLatin1(kReleaseApi))};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("LunaDah/%1").arg(LUNADAH_VERSION));
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
  const auto error = reply_->error();
  const QByteArray body = reply_->readAll();
  const auto finalUrl = reply_->url();
  reply_->deleteLater();
  reply_ = nullptr;

  if (error != QNetworkReply::NoError) {
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
    finishWithError("GitHub returned invalid release information.");
    return;
  }
  const auto object = document.object();
  latestVersion_ = object.value("tag_name").toString();
  releaseUrl_ = object.value("html_url").toString();
  const QUrl release(releaseUrl_);
  if (versionParts(latestVersion_).isEmpty() || release.scheme() != "https" ||
      release.host() != "github.com" ||
      !release.path().startsWith("/LuYishan-4/LuDash/releases/")) {
    finishWithError("GitHub returned unexpected release information.");
    return;
  }
  status_ = isNewer(latestVersion_, QStringLiteral(LUNADAH_VERSION))
                ? "available"
                : "upToDate";
  error_.clear();
  checkedAt_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  emit changed();
}

} // namespace LuDash
