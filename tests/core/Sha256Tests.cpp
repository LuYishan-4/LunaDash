#include "core/crypto/Sha256.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  if (LunaDash::sha256Hex("") !=
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ||
      LunaDash::sha256Hex("abc") !=
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
    std::cerr << "SHA-256 known-answer test failed\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
