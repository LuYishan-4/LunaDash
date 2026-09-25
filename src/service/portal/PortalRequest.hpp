#pragma once

#include <QObject>
#include <QPointer>

class QDialog;

namespace LunaDash {
class PortalRequest final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
  explicit PortalRequest(QDialog *dialog, QObject *parent = nullptr);

public slots:
  void Close();

private:
  QPointer<QDialog> dialog_;
};
} // namespace LunaDash
