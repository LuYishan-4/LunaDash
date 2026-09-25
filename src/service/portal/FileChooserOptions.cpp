#include "service/portal/FileChooserOptions.hpp"
#include "service/portal/FilePickerDialog.hpp"
#include <QDBusMetaType>
#include <QFile>
#include <QMimeDatabase>
#include <QUrl>

namespace LunaDash {
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilterRule &value) {
  arg.beginStructure();
  arg << value.type << value.value;
  arg.endStructure();
  return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PortalFilterRule &value) {
  arg.beginStructure();
  arg >> value.type >> value.value;
  arg.endStructure();
  return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFileFilter &value) {
  arg.beginStructure();
  arg << value.label << value.rules;
  arg.endStructure();
  return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PortalFileFilter &value) {
  arg.beginStructure();
  arg >> value.label >> value.rules;
  arg.endStructure();
  return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const PortalChoiceValue &value) {
  arg.beginStructure();
  arg << value.id << value.label;
  arg.endStructure();
  return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PortalChoiceValue &value) {
  arg.beginStructure();
  arg >> value.id >> value.label;
  arg.endStructure();
  return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFileChoice &value) {
  arg.beginStructure();
  arg << value.id << value.label << value.values << value.selected;
  arg.endStructure();
  return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PortalFileChoice &value) {
  arg.beginStructure();
  arg >> value.id >> value.label >> value.values >> value.selected;
  arg.endStructure();
  return arg;
}

void configureFilePicker(FilePickerDialog &picker, const QVariantMap &options) {
  qDBusRegisterMetaType<PortalFilterRule>();
  qDBusRegisterMetaType<PortalFileFilter>();
  qDBusRegisterMetaType<QList<PortalFileFilter>>();
  qDBusRegisterMetaType<PortalChoiceValue>();
  qDBusRegisterMetaType<QList<PortalChoiceValue>>();
  qDBusRegisterMetaType<PortalFileChoice>();
  qDBusRegisterMetaType<QList<PortalFileChoice>>();
  const auto filters =
      qdbus_cast<QList<PortalFileFilter>>(options.value("filters"));
  const auto current =
      qdbus_cast<PortalFileFilter>(options.value("current_filter"));
  QList<QPair<QString, QStringList>> displayFilters;
  int selected = 0;
  QMimeDatabase mime;
  for (const auto &filter : filters) {
    if (filter.label == current.label)
      selected = displayFilters.size();
    QStringList patterns;
    for (const auto &rule : filter.rules) {
      if (rule.type == 0)
        patterns.append(rule.value);
      else if (rule.type == 1) {
        if (rule.value.endsWith("/*")) {
          const auto prefix = rule.value.chopped(1);
          for (const auto &type : mime.allMimeTypes())
            if (type.name().startsWith(prefix))
              patterns.append(type.globPatterns());
        } else
          patterns.append(mime.mimeTypeForName(rule.value).globPatterns());
      }
    }
    patterns.removeDuplicates();
    displayFilters.append({filter.label, patterns});
  }
  picker.setFilters(displayFilters, selected);
  const auto choices =
      qdbus_cast<QList<PortalFileChoice>>(options.value("choices"));
  for (const auto &choice : choices) {
    QList<QPair<QString, QString>> values;
    for (const auto &value : choice.values)
      values.append({value.id, value.label});
    picker.addChoice(choice.id, choice.label, values, choice.selected);
  }
}

QVariantMap filePickerResults(const FilePickerDialog &picker,
                              const QVariantMap &options) {
  QStringList uris;
  for (const auto &path : picker.selectedPaths())
    uris.append(QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
  QVariantMap result{{"uris", uris}};
  const auto filters =
      qdbus_cast<QList<PortalFileFilter>>(options.value("filters"));
  const int selected = picker.selectedFilter();
  if (selected >= 0 && selected < filters.size())
    result["current_filter"] = QVariant::fromValue(filters.at(selected));
  QList<PortalChoiceValue> choices;
  for (const auto &choice : picker.selectedChoices())
    choices.append({choice.first, choice.second});
  if (options.contains("choices"))
    result["choices"] = QVariant::fromValue(choices);
  return result;
}

QStringList portalSaveFileNames(const QVariantMap &options) {
  const auto files = qdbus_cast<QList<QByteArray>>(options.value("files"));
  QStringList names;
  for (auto bytes : files) {
    if (bytes.endsWith('\0'))
      bytes.chop(1);
    const auto name = QFile::decodeName(bytes);
    // SaveFiles supplies basenames, never paths outside the chosen folder.
    if (name.isEmpty() || name == "." || name == ".." || name.contains('/') ||
        name.contains(QChar::Null))
      return {};
    names.append(name);
  }
  return names;
}
} // namespace LunaDash
