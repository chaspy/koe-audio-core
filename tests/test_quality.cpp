// Port of macos/Tests/quality/main.swift (koe-dev): synthetic sine waves and noise only.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

#include "koe_audio.h"
#include "quality.hpp"

static int failures = 0;
#define CHECK(cond, msg)                                              \
  do {                                                                \
    if (!(cond)) {                                                    \
      ++failures;                                                     \
      std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);     \
    }                                                                 \
  } while (0)

// 16 kHz. 0.5 s of room noise (+-noise) before and after, a 220 Hz sine in between
// whose RMS is speech_db dBFS.
static std::vector<int16_t> recording(double speech_db, double seconds, int noise = 3) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(-noise, noise);
  std::vector<int16_t> quiet(8000);
  for (auto &s : quiet) s = static_cast<int16_t>(dist(rng));
  const double amp = 32768 * std::pow(10.0, speech_db / 20) * std::sqrt(2.0);
  std::vector<int16_t> out = quiet;
  const int n = static_cast<int>(seconds * 16000);
  for (int i = 0; i < n; ++i) {
    const double v = amp * std::sin(2 * std::numbers::pi * 220 * i / 16000);
    out.push_back(static_cast<int16_t>(std::max(-32767.0, std::min(32767.0, v))));
  }
  out.insert(out.end(), quiet.begin(), quiet.end());
  return out;
}

static koe_quality measure(const std::vector<int16_t> &pcm) {
  koe_quality q;
  CHECK(koe_measure_i16(pcm.data(), pcm.size(), 16000, &q) == KOE_OK, "measure returns OK");
  return q;
}

static bool near(double a, double b, double tol) { return std::fabs(a - b) < tol; }

int main() {
  // -50 dBFS: level about -50, warns, speech_s is the tone length
  const koe_quality soft = measure(recording(-50, 2));
  CHECK(soft.valid && near(soft.level_db, -50, 1), "-50 dBFS is measured as about -50");
  CHECK(koe_too_quiet(&soft), "-50 dBFS warns");
  CHECK(near(soft.speech_s, 2, 0.1), "speech_s is the tone length");

  // -25 dBFS: no warning
  const koe_quality normal = measure(recording(-25, 2));
  CHECK(normal.valid && near(normal.level_db, -25, 1), "-25 dBFS is measured as about -25");
  CHECK(!koe_too_quiet(&normal), "-25 dBFS does not warn");

  // threshold: -44 does not warn, -46 does
  const koe_quality q44 = measure(recording(-44, 2));
  const koe_quality q46 = measure(recording(-46, 2));
  CHECK(!koe_too_quiet(&q44), "-44 dBFS does not warn");
  CHECK(koe_too_quiet(&q46), "-46 dBFS warns");

  // short sound (0.2 s) does not warn even when quiet
  const koe_quality shortq = measure(recording(-50, 0.2));
  CHECK(!koe_too_quiet(&shortq), "short sound does not warn");

  // noise only: no level
  CHECK(!measure(recording(-120, 1)).valid, "noise only has no level");

  // below the -56 dBFS floor: not voice
  CHECK(!measure(recording(-60, 2)).valid, "-60 dBFS is below the floor");

  // the decision looks only at the values
  koe_quality v{};
  v.valid = 1; v.level_db = -50; v.speech_s = 1;
  CHECK(koe_too_quiet(&v), "values: quiet");
  v.speech_s = 0;
  CHECK(!koe_too_quiet(&v), "no speech_s: no warning");
  CHECK(!koe_too_quiet(nullptr), "NULL: no warning");
  const koe_quality empty{};
  CHECK(!koe_too_quiet(&empty), "empty");

  // extra fields on a clean tone
  CHECK(soft.noise_db < -70 && soft.snr_db > 10, "noise and SNR are plausible");
  CHECK(soft.clip_ratio == 0, "no clipping");

  // C API argument checks
  koe_quality q;
  CHECK(koe_measure_i16(nullptr, 0, 16000, &q) == KOE_OK && !q.valid, "empty input is OK and invalid");
  CHECK(koe_measure_i16(nullptr, 5, 16000, &q) == KOE_ERR_INVALID_ARG, "NULL with length");
  CHECK(koe_measure_i16(nullptr, 0, 16000, nullptr) == KOE_ERR_INVALID_ARG, "NULL out");
  const std::vector<int16_t> pcm = recording(-25, 1);
  CHECK(koe_measure_i16(pcm.data(), pcm.size(), 0, &q) == KOE_ERR_INVALID_ARG, "bad sample rate");

  // float input agrees with int16 input; NaN/inf do not break it
  const std::vector<int16_t> rec = recording(-25, 2);
  std::vector<float> f(rec.size());
  for (std::size_t i = 0; i < rec.size(); ++i) f[i] = static_cast<float>(rec[i]) / 32768.0f;
  koe_quality qf;
  CHECK(koe_measure_f32(f.data(), f.size(), 16000, &qf) == KOE_OK, "float OK");
  CHECK(near(qf.level_db, normal.level_db, 0.11), "float matches int16");
  f[100] = std::nanf("");
  f[101] = INFINITY;
  CHECK(koe_measure_f32(f.data(), f.size(), 16000, &qf) == KOE_OK && qf.valid, "NaN/inf input is handled");

  // clipping is counted (a 0 dBFS sine is clamped to full scale)
  CHECK(measure(recording(0, 1)).clip_ratio > 0.1, "clipped tone has a clip_ratio");

  if (failures > 0) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("quality: ok\n");
  return 0;
}
