#pragma once
#include <QStyledItemDelegate>
namespace LunaDash {
class FileIconDelegate final : public QStyledItemDelegate {
public:
  explicit FileIconDelegate(QObject *parent);

protected:
  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override;
};
} // namespace LunaDash
