#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

struct SystemSettingsTool {
  std::string id;
  std::string category;
  std::string name;
  std::string package;
  bool available = false;
  bool configurable = false;
  bool host = false;
};

std::vector<SystemSettingsTool> systemSettingsTools();
bool systemSettingsToolUsesHost(std::string_view id);
std::vector<std::string> systemSettingsCommand(std::string_view id);
// The graphical network connection editor, or nmtui in a terminal; empty when
// neither is installed.
std::vector<std::string> networkConnectionEditorCommand();

} // namespace LunaDash
