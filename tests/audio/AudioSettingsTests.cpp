#include "desktop/audio/AudioSettings.hpp"

#include <cassert>

int main() {
  using LunaDash::audioCommand;
  using LunaDash::parseAudioDevices;
  using LunaDash::parseAudioVolume;

  const auto volume = parseAudioVolume("Volume: 0.63\n");
  assert(volume.at("available") == true);
  assert(volume.at("volume") == 63);
  assert(volume.at("muted") == false);

  const auto muted = parseAudioVolume("Volume: 0.27 [MUTED]\n");
  assert(muted.at("available") == true);
  assert(muted.at("volume") == 27);
  assert(muted.at("muted") == true);
  assert(parseAudioVolume("not installed").at("available") == false);

  const auto devices =
      parseAudioDevices("Audio\n"
                        " ├─ Sinks:\n"
                        " │  * 52. Built-in Analog Stereo [vol: 0.63]\n"
                        " │    88. HDMI Output [vol: 1.00]\n"
                        " ├─ Sources:\n"
                        " │  * 52. Built-in Microphone [vol: 0.50]\n");
  assert(devices.size() == 2);
  assert(devices.at(0).at("id") == 52);
  assert(devices.at(0).at("name") == "Built-in Analog Stereo");
  assert(devices.at(0).at("default") == true);
  assert(devices.at(1).at("id") == 88);
  assert(devices.at(1).at("default") == false);

  assert((audioCommand({{"device", "output"}, {"volume", 45}}) ==
          std::vector<std::string>{"set-volume", "@DEFAULT_AUDIO_SINK@", "45%",
                                   "--limit", "1.0"}));
  assert((audioCommand({{"device", "input"}, {"mute", true}}) ==
          std::vector<std::string>{"set-mute", "@DEFAULT_AUDIO_SOURCE@", "1"}));
  assert((audioCommand({{"device", "output"}, {"id", 88}}) ==
          std::vector<std::string>{"set-default", "88"}));
  assert(audioCommand({{"device", "output"}, {"volume", 145}}).empty());
  assert(audioCommand({{"device", "input"}, {"id", 3}}).empty());
}
