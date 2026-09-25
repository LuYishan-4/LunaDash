#include "service/portal/PortalRequest.hpp"

#include <QDialog>

namespace LunaDash {
PortalRequest::PortalRequest(QDialog *dialog, QObject *parent)
    : QObject(parent), dialog_(dialog) {}

void PortalRequest::Close() {
  if (dialog_)
    dialog_->reject();
}
} // namespace LunaDash
