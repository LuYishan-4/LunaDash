#include "core/crypto/Sha256.hpp"

#include <gio/gio.h>

#include <limits>
#include <memory>
#include <stdexcept>

namespace LunaDash {

std::string sha256Hex(std::string_view data) {
  if (data.size() > static_cast<std::size_t>(
                        std::numeric_limits<gssize>::max()))
    throw std::length_error("SHA-256 input is too large");

  using Checksum =
      std::unique_ptr<GChecksum, decltype(&g_checksum_free)>;
  Checksum checksum(g_checksum_new(G_CHECKSUM_SHA256), g_checksum_free);
  if (!checksum)
    throw std::runtime_error("Could not allocate SHA-256 checksum");

  g_checksum_update(checksum.get(),
                    reinterpret_cast<const guchar *>(data.data()),
                    static_cast<gssize>(data.size()));
  const char *digest = g_checksum_get_string(checksum.get());
  if (!digest)
    throw std::runtime_error("Could not calculate SHA-256 checksum");
  return digest;
}

} // namespace LunaDash
