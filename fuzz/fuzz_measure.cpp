// libFuzzer target: any byte string, any sample rate, must not crash or trip a sanitizer.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "koe_audio.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return 0;
  const int rates[] = {8000, 16000, 44100, 48000};
  const int rate = rates[data[0] % 4];
  const bool use_float = (data[1] & 1) != 0;
  data += 2;
  size -= 2;
  koe_quality q;
  if (use_float) {
    std::vector<float> pcm(size / sizeof(float));
    if (!pcm.empty()) std::memcpy(pcm.data(), data, pcm.size() * sizeof(float));
    koe_measure_f32(pcm.data(), pcm.size(), rate, &q);
  } else {
    std::vector<int16_t> pcm(size / sizeof(int16_t));
    if (!pcm.empty()) std::memcpy(pcm.data(), data, pcm.size() * sizeof(int16_t));
    koe_measure_i16(pcm.data(), pcm.size(), rate, &q);
  }
  koe_too_quiet(&q);
  return 0;
}
