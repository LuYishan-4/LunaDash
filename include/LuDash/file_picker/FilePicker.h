#pragma once

#include <QSize>
#include <QString>

class QWidget;

namespace LuDash {

inline constexpr qint64 maximumImageFileBytes = 64 * 1024 * 1024;
inline constexpr qint64 maximumImagePixels = 32 * 1024 * 1024;

bool isEligibleImageFile(const QString &path);
QSize boundedPreviewSize(const QSize &sourceSize, const QSize &bounds);
QString selectImageFile(QWidget *parent, const QString &initialPath = {});

} // namespace LuDash
