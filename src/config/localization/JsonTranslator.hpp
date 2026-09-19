#pragma once
#include <QJsonObject>
#include <QTranslator>
namespace LunaDash {
class JsonTranslator final : public QTranslator {
public:
  JsonTranslator(const QString &language, QObject *parent);
  QString translate(const char *context, const char *source,
                    const char *disambiguation, int count) const override;
  bool isEmpty() const override;

private:
  QJsonObject messages_;
};
} // namespace LunaDash
