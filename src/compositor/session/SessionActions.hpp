#pragma once

#include <map>
#include <string>
#include <string_view>

namespace LunaDash {

struct SessionActionSnapshot {
  std::map<std::string, bool> availability;
  std::string error;
};

bool isValidSessionAction(std::string_view action);

class SessionActions final {
public:
  SessionActions();

  SessionActionSnapshot snapshot() const;
  bool execute(std::string_view action, std::string *error = nullptr);

private:
  void refreshAvailability();

  std::map<std::string, bool> availability_;
  std::string availabilityError_;
};

} // namespace LunaDash
