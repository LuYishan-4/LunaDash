#include "service/portal/FileIcons.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
namespace LunaDash {
QIcon fileIcon(FileIcon icon, const QColor &color) {
  const auto key = QString("ludash-icon-%1-%2")
                       .arg(static_cast<int>(icon))
                       .arg(color.name(QColor::HexArgb));
  QPixmap pixmap;
  if (QPixmapCache::find(key, &pixmap))
    return QIcon(pixmap);
  pixmap = QPixmap(96, 96);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.scale(4, 4);
  painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  auto line = [&painter](qreal x1, qreal y1, qreal x2, qreal y2) {
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
  };
  auto box = [&painter](qreal x, qreal y, qreal w, qreal h) {
    painter.drawRoundedRect(QRectF(x, y, w, h), 2, 2);
  };
  switch (icon) {
  case FileIcon::Folder: {
    QPainterPath path;
    path.moveTo(3, 7);
    path.quadTo(3, 5, 5, 5);
    path.lineTo(9, 5);
    path.lineTo(12, 8);
    path.lineTo(19, 8);
    path.quadTo(21, 8, 21, 10);
    path.lineTo(21, 18);
    path.quadTo(21, 20, 19, 20);
    path.lineTo(5, 20);
    path.quadTo(3, 20, 3, 18);
    path.closeSubpath();
    QColor fill(color);
    fill.setAlpha(28);
    painter.fillPath(path, fill);
    painter.drawPath(path);
    line(3, 10, 21, 10);
    break;
  }
  case FileIcon::Home:
    line(3, 11, 12, 4);
    line(12, 4, 21, 11);
    line(6, 10, 6, 20);
    line(6, 20, 18, 20);
    line(18, 20, 18, 10);
    line(10, 20, 10, 14);
    line(10, 14, 14, 14);
    line(14, 14, 14, 20);
    break;
  case FileIcon::File:
    box(5, 3, 14, 18);
    line(9, 9, 15, 9);
    line(9, 13, 15, 13);
    line(9, 17, 12, 17);
    break;
  case FileIcon::Desktop:
    box(3, 4, 18, 13);
    line(12, 17, 12, 21);
    line(8, 21, 16, 21);
    break;
  case FileIcon::Download:
    line(12, 3, 12, 15);
    line(7, 10, 12, 15);
    line(12, 15, 17, 10);
    line(4, 16, 4, 20);
    line(4, 20, 20, 20);
    line(20, 20, 20, 16);
    break;
  case FileIcon::Picture:
    box(3, 4, 18, 16);
    painter.drawEllipse(QPointF(8, 9), 1.5, 1.5);
    line(4, 18, 11, 12);
    line(11, 12, 15, 16);
    line(15, 16, 18, 13);
    line(18, 13, 21, 16);
    break;
  case FileIcon::Music:
    line(10, 17, 10, 5);
    line(10, 5, 19, 3);
    line(19, 3, 19, 15);
    painter.drawEllipse(QRectF(4, 16, 6, 5));
    painter.drawEllipse(QRectF(13, 14, 6, 5));
    break;
  case FileIcon::Video:
    box(3, 5, 13, 14);
    line(16, 10, 21, 7);
    line(21, 7, 21, 17);
    line(21, 17, 16, 14);
    break;
  case FileIcon::Drive:
    box(3, 7, 18, 12);
    line(3, 14, 21, 14);
    line(7, 17, 8, 17);
    line(17, 17, 18, 17);
    break;
  case FileIcon::Back:
    line(16, 5, 9, 12);
    line(9, 12, 16, 19);
    break;
  case FileIcon::Forward:
    line(8, 5, 15, 12);
    line(15, 12, 8, 19);
    break;
  case FileIcon::Up:
    line(5, 12, 12, 5);
    line(12, 5, 19, 12);
    line(12, 5, 12, 20);
    break;
  case FileIcon::Grid:
    box(4, 4, 6, 6);
    box(14, 4, 6, 6);
    box(4, 14, 6, 6);
    box(14, 14, 6, 6);
    break;
  case FileIcon::List:
    for (int y : {6, 12, 18}) {
      line(4, y, 5, y);
      line(9, y, 20, y);
    }
    break;
  case FileIcon::Plus:
    line(5, 12, 19, 12);
    line(12, 5, 12, 19);
    break;
  case FileIcon::Copy:
    box(8, 8, 12, 13);
    line(15, 4, 4, 4);
    line(4, 4, 4, 16);
    break;
  case FileIcon::Cut:
    painter.drawEllipse(QRectF(3, 15, 6, 6));
    painter.drawEllipse(QRectF(15, 15, 6, 6));
    line(6, 15, 18, 3);
    line(18, 15, 6, 3);
    break;
  case FileIcon::Paste:
    box(5, 5, 14, 16);
    painter.setBrush(QColor("#1a222d"));
    box(9, 3, 6, 4);
    break;
  case FileIcon::Rename:
    line(4, 20, 9, 19);
    line(9, 19, 20, 8);
    line(20, 8, 16, 4);
    line(16, 4, 5, 15);
    line(5, 15, 4, 20);
    line(13, 7, 17, 11);
    break;
  case FileIcon::Trash:
    line(4, 6, 20, 6);
    line(9, 3, 15, 3);
    line(6, 6, 7, 21);
    line(7, 21, 17, 21);
    line(17, 21, 18, 6);
    line(10, 10, 10, 17);
    line(14, 10, 14, 17);
    break;
  case FileIcon::Search:
    painter.drawEllipse(QRectF(3, 3, 12, 12));
    line(14, 14, 21, 21);
    break;
  case FileIcon::Eye: {
    QPainterPath path;
    path.moveTo(2, 12);
    path.quadTo(12, 1, 22, 12);
    path.quadTo(12, 23, 2, 12);
    painter.drawPath(path);
    painter.drawEllipse(QPointF(12, 12), 3, 3);
    break;
  }
  case FileIcon::Terminal:
    box(3, 4, 18, 16);
    line(7, 9, 10, 12);
    line(10, 12, 7, 15);
    line(12, 15, 17, 15);
    break;
  }
  painter.end();
  QPixmapCache::insert(key, pixmap);
  return QIcon(pixmap);
}
} // namespace LunaDash
