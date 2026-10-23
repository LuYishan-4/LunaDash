#pragma once

#include "shell/modules/ShellModuleSchema.hpp"

#include <functional>
#include <string>
#include <string_view>

struct _GFileMonitor;
typedef struct _GFileMonitor GFileMonitor;

namespace LunaDash {

class ShellModules final {
public:
  ShellModules();
  ~ShellModules();
  ShellModules(const ShellModules &) = delete;
  ShellModules &operator=(const ShellModules &) = delete;

  ShellModuleJson snapshot() const;
  bool validate(std::string_view text, std::string *error);
  bool apply(std::string_view text, std::string *error);
  bool reset(std::string *error);
  void reportError(std::string_view id, std::string_view error);
  int panelExtent(int fallbackHeight) const;
  std::string panelEdge() const;
  bool panelAtBottom() const;
  void setChangedCallback(std::function<void()> callback);

private:
  std::string path_;
  std::string status_;
  ShellModuleJson document_;
  ShellModuleJson errors_ = ShellModuleJson::object();
  GFileMonitor *monitor_ = nullptr;
  unsigned int debounceSource_ = 0;
  int revision_ = 0;
  std::function<void()> changedCallback_;

  void reload();
};

} // namespace LunaDash
