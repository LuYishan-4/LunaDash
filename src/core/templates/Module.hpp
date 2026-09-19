#pragma once

#include <concepts>

namespace LunaDash::Templates {
// Executables own their application module for the entire event-loop lifetime.
template <typename T>
concept Module = requires(T &module, int argc, char **argv) {
  { module.run(argc, argv) } -> std::same_as<int>;
};

template <Module T> int runModule(int argc, char **argv) {
  T module;
  return module.run(argc, argv);
}
} // namespace LunaDash::Templates
