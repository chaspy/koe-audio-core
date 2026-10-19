// Stand-in driver when libFuzzer is unavailable: feeds the fuzz target
// deterministic pseudo-random inputs of varied length.
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main() try {
  std::mt19937 rng(12345);  // fixed seed: the smoke run must be reproducible
  for (int i = 0; i < 300; ++i) {
    const std::size_t n = (i % 3 == 0) ? rng() % 64 : rng() % 120000;
    std::vector<uint8_t> buf(n);
    for (auto &b : buf) b = static_cast<uint8_t>(rng());
    LLVMFuzzerTestOneInput(buf.data(), buf.size());
  }
  return 0;
} catch (...) {
  return 1;
}
