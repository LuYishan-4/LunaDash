#include "config/localization/JsonTranslator.hpp"
#include "config/localization/Localization.hpp"

namespace LunaDash {
JsonTranslator::JsonTranslator(const QString &language, QObject *parent)
    : QTranslator(parent) {
  messages_ = languageDictionary(language);
}
QString JsonTranslator::translate(const char *context, const char *source,
                                  const char *, int) const {
  if (QByteArray(context) != "LuDash")
    return {};
  return messages_.value(QString::fromUtf8(source)).toString();
}
bool JsonTranslator::isEmpty() const { return messages_.isEmpty(); }

} // namespace LunaDash
