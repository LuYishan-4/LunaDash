#include <LuDash/update_check/UpdateChecker.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace LuDash {
namespace {
constexpr auto kReleaseApi = "https://api.github.com/repos/LuYishan-4/LunaDash/releases/latest";
constexpr auto kDevCommitApi = "https://api.github.com/repos/LuYishan-4/LunaDash/commits/dev";
constexpr auto kRepositoryUrl = "https://github.com/LuYishan-4/LunaDash";
constexpr qsizetype kMaximumReplyBytes = 256 * 1024;
QList<int> versionParts(QString version) { if (version.startsWith('v')) version.remove(0, 1); QList<int> parts; for (const auto &part : version.split('.')) { bool ok=false; const int n=part.section('-',0,0).toInt(&ok); if(!ok||n<0)return {}; parts.append(n);} return parts; }
bool isNewer(const QString &latest,const QString &current){auto l=versionParts(latest),r=versionParts(current);if(l.isEmpty()||r.isEmpty())return false;while(l.size()<r.size())l.append(0);while(r.size()<l.size())r.append(0);for(qsizetype i=0;i<l.size();++i){if(l[i]!=r[i])return l[i]>r[i];}return false;}
QString selectedChannel(){const auto value=QSettings().value("desktop/updateChannel","stable").toString();return value=="dev"?QStringLiteral("dev"):QStringLiteral("stable");}
}
UpdateChecker::UpdateChecker(QObject *parent):QObject(parent),network_(new QNetworkAccessManager(this)),timeout_(new QTimer(this)){channel_=selectedChannel();timeout_->setSingleShot(true);timeout_->setInterval(8000);connect(timeout_,&QTimer::timeout,this,[this]{if(reply_)reply_->abort();finishWithError("The update check timed out.");});}
QJsonObject UpdateChecker::snapshot()const{return{{"status",status_},{"channel",channel_},{"currentVersion",QStringLiteral(LUNADASH_VERSION)},{"currentCommit",QStringLiteral(LUNADASH_GIT_COMMIT)},{"latestVersion",latestVersion_},{"latestCommit",latestCommit_},{"latestMessage",latestMessage_},{"releaseUrl",releaseUrl_},{"repositoryUrl",QString::fromLatin1(kRepositoryUrl)},{"error",error_},{"checkedAt",checkedAt_}};}
bool UpdateChecker::setChannel(const QString &channel){if(channel!="stable"&&channel!="dev")return false;channel_=channel;QSettings().setValue("desktop/updateChannel",channel_);status_="idle";latestVersion_.clear();latestCommit_.clear();latestMessage_.clear();releaseUrl_.clear();error_.clear();emit changed();return true;}
void UpdateChecker::check(){if(reply_)return;channel_=selectedChannel();status_="checking";latestVersion_.clear();latestCommit_.clear();latestMessage_.clear();releaseUrl_.clear();error_.clear();emit changed();QNetworkRequest request{QUrl(QString::fromLatin1(channel_=="dev"?kDevCommitApi:kReleaseApi))};request.setHeader(QNetworkRequest::UserAgentHeader,QStringLiteral("LunaDash/%1").arg(LUNADASH_VERSION));request.setRawHeader("Accept","application/vnd.github+json");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);reply_=network_->get(request);connect(reply_,&QNetworkReply::finished,this,&UpdateChecker::finishReply);connect(reply_,&QIODevice::readyRead,this,[this]{if(reply_&&reply_->bytesAvailable()>kMaximumReplyBytes)reply_->abort();});timeout_->start();}
void UpdateChecker::finishWithError(const QString &message){timeout_->stop();if(reply_){reply_->deleteLater();reply_=nullptr;}status_="error";error_=message;checkedAt_=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);emit changed();}
void UpdateChecker::finishReply(){if(!reply_)return;timeout_->stop();const auto networkError=reply_->error();const QByteArray body=reply_->readAll();const auto finalUrl=reply_->url();reply_->deleteLater();reply_=nullptr;if(networkError!=QNetworkReply::NoError){finishWithError("GitHub could not be reached.");return;}if(body.size()>kMaximumReplyBytes||finalUrl.scheme()!="https"||finalUrl.host()!="api.github.com"){finishWithError("The update response was rejected.");return;}QJsonParseError parseError;const auto document=QJsonDocument::fromJson(body,&parseError);if(parseError.error!=QJsonParseError::NoError||!document.isObject()){finishWithError("GitHub returned invalid update information.");return;}const auto object=document.object();if(channel_=="dev"){latestCommit_=object.value("sha").toString();latestMessage_=object.value("commit").toObject().value("message").toString().section('\n',0,0);releaseUrl_=object.value("html_url").toString();const QUrl url(releaseUrl_);if(latestCommit_.size()<12||url.scheme()!="https"||url.host()!="github.com"||!url.path().startsWith("/LuYishan-4/LunaDash/commit/")){finishWithError("GitHub returned unexpected commit information.");return;}const QString current=QStringLiteral(LUNADASH_GIT_COMMIT);status_=!current.isEmpty()&&current!="unknown"&&latestCommit_.startsWith(current)?"upToDate":"available";}else{latestVersion_=object.value("tag_name").toString();releaseUrl_=object.value("html_url").toString();const QUrl url(releaseUrl_);if(versionParts(latestVersion_).isEmpty()||url.scheme()!="https"||url.host()!="github.com"||!url.path().startsWith("/LuYishan-4/LunaDash/releases/")){finishWithError("GitHub returned unexpected release information.");return;}status_=isNewer(latestVersion_,QStringLiteral(LUNADASH_VERSION))?"available":"upToDate";}error_.clear();checkedAt_=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);emit changed();}
} // namespace LuDash
