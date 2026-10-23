#include "koe_audio.h"

_Static_assert(KOE_AUDIO_API_VERSION == 1, "Review ABI assertions for a new API version");
_Static_assert(sizeof(koe_quality) == 56, "koe_quality size changed");
_Static_assert(offsetof(koe_quality, valid) == 0, "valid offset changed");
_Static_assert(offsetof(koe_quality, level_db) == 8, "level_db offset changed");
_Static_assert(offsetof(koe_quality, noise_db) == 16, "noise_db offset changed");
_Static_assert(offsetof(koe_quality, snr_db) == 24, "snr_db offset changed");
_Static_assert(offsetof(koe_quality, speech_s) == 32, "speech_s offset changed");
_Static_assert(offsetof(koe_quality, clip_ratio) == 40, "clip_ratio offset changed");
_Static_assert(offsetof(koe_quality, brightness) == 48, "brightness offset changed");

int main(void) {
  koe_quality q = {0};
  const int16_t pcm_i16[160] = {0};
  const float pcm_f32[160] = {0};
  if (koe_audio_api_version() != KOE_AUDIO_API_VERSION) return 1;
  if (koe_measure_i16(pcm_i16, 160, 16000, &q) != KOE_OK || q.valid != 0) return 2;
  if (koe_measure_f32(pcm_f32, 160, 16000, &q) != KOE_OK || q.valid != 0) return 3;
  if (koe_too_quiet(&q) != 0) return 4;
  q.valid = 1;
  q.level_db = -50.0;
  q.speech_s = 0.5;
  if (koe_too_quiet(&q) != 1) return 5;
  return 0;
}
