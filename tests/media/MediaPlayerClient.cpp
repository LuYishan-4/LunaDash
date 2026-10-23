#include "shell/media/MediaPlayer.hpp"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  nlohmann::json result;
  if (argc >= 2 && std::string(argv[1]) == "media-status") {
    result = LunaDash::mediaStatus(argc > 2 ? argv[2] : "",
                                   argc > 3 ? argv[3] : "");
  } else if (argc >= 3 && std::string(argv[1]) == "media-action") {
    result = LunaDash::mediaAction(argc > 2 ? argv[2] : "",
                                   argc > 3 ? argv[3] : "",
                                   argc > 4 ? argv[4] : "",
                                   argc > 5 ? argv[5] : "",
                                   argc > 6 ? argv[6] : "");
  } else {
    result = {{"error", "Expected media-status or media-action."}};
  }
  std::cout << result.dump() << '\n';
  return result.contains("error") ? 2 : 0;
}
