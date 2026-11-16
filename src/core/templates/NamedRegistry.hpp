#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace LunaDash::Templates {

// Entries registered by name. Code that executes an entry only looks it up;
// the entries themselves come from their owning domain (or a plugin) through
// add(). A name is registered once: the first registration wins.
template <typename Entry> class NamedRegistry {
public:
  bool add(std::string name, Entry entry) {
    if (name.empty())
      return false;
    return entries_.emplace(std::move(name), std::move(entry)).second;
  }

  bool remove(std::string_view name) {
    const auto found = entries_.find(name);
    if (found == entries_.end())
      return false;
    entries_.erase(found);
    return true;
  }

  const Entry *find(std::string_view name) const {
    const auto found = entries_.find(name);
    return found == entries_.end() ? nullptr : &found->second;
  }

  bool contains(std::string_view name) const { return find(name) != nullptr; }

  std::vector<std::string> names() const {
    std::vector<std::string> result;
    result.reserve(entries_.size());
    for (const auto &[name, entry] : entries_)
      result.push_back(name);
    return result;
  }

private:
  std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace LunaDash::Templates
