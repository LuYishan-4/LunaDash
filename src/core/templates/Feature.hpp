#pragma once
#include <concepts>

namespace LunaDash::Templates {
// Static lifecycle contract. A failed initialization must still allow shutdown.
template <typename T>
concept Feature = requires(T &feature) {
  { feature.initialize() } -> std::same_as<bool>;
  { feature.shutdown() } -> std::same_as<void>;
};
} // namespace LunaDash::Templates
