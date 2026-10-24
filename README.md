# koe-audio-core

A C++20 library for loudness and silence assessment in [Koe](https://github.com/chaspy/koe).
It exposes a C API in `include/koe_audio.h`, allowing Swift to call the same audio analysis
implementation. Integration with Go through cgo is planned.

## Build and test

Requirements: CMake 3.20 or newer and a C++20 compiler.

Optional: `kagami` (the maintainer's local quality gate).

```sh
bin/ci
```

This configures and builds the library, runs CTest, repeats the tests with AddressSanitizer
and UndefinedBehaviorSanitizer, and runs `kagami check --format agent-brief` when available
on `PATH`. If kagami is absent, the optional quality gate is skipped; if present, its
failures still cause `bin/ci` to fail.
Builds and tests use at most two parallel jobs.

To build and run the tests separately:

```sh
cmake -S . -B build/default
cmake --build build/default -j 2
ctest --test-dir build/default -j 2 --output-on-failure
```

The static library is `build/default/libkoe_audio.a`. Link it with the C++ runtime when
calling it from another language. Swift integration can expose the header through a
bridging header or module map; a Swift package wrapper is not provided here.

## C API compatibility

`KOE_AUDIO_API_VERSION` is currently `1`; `koe_audio_api_version()` returns the version
of the linked library. Existing function signatures and structure layouts remain stable
within an API version. Breaking changes require incrementing the version; compatible
API additions do not. The C17 test fixes `koe_quality` at 56 bytes with field offsets
0, 8, 16, 24, 32, 40, and 48 on the supported 64-bit macOS and Windows targets.

## Windows cross-build (MinGW)

With the x86_64 MinGW-w64 C/C++ compilers and binutils on `PATH`:

```sh
cmake -S . -B build/mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build/mingw --target koe_audio test_c_api -j 2
```

The static library is `build/mingw/libkoe_audio.a`. Link with the MinGW C++ runtime.
The C17 ABI test is compiled and linked for Windows, but is not run on the build host.
`bin/ci` also performs this cross-build and uses `x86_64-w64-mingw32-nm` to verify
all public C function symbols are defined without name mangling. If
`x86_64-w64-mingw32-g++` is absent, it prints `skipping`; no toolchain is installed.

## C API example

The following complete C example uses a synthetic waveform: silence followed by a tone.

```c
#include "koe_audio.h"
#include <stdio.h>

int main(void) {
  int16_t pcm[16000] = {0};
  for (size_t i = 4800; i < 16000; ++i) {
    pcm[i] = (i % 40 < 20) ? 2000 : -2000;
  }

  koe_quality q = {0};
  int status = koe_measure_i16(pcm, 16000, 16000, &q);
  if (status != KOE_OK) return 1;
  if (q.valid) {
    printf("%.1f dBFS, %.2f s\n", q.level_db, q.speech_s);
    if (koe_too_quiet(&q)) puts("Input volume is too low.");
  } else {
    puts("Too little input or no voiced windows detected.");
  }
  return 0;
}
```

Save this as `example.c` and, after building, compile and link it:

```sh
cc -std=c11 -Iinclude -c example.c -o example.o
c++ example.o build/default/libkoe_audio.a -o example
./example
```

Input is mono PCM at 8,000–192,000 Hz. The caller owns all buffers; the library only reads
samples during the call and retains no pointers. `koe_measure_f32` accepts floating-point
samples with full scale ±1.0, replaces NaN with zero, and clamps out-of-range samples.
A null sample pointer is allowed only for zero-length input. The output pointer must be
non-null. Invalid arguments return `KOE_ERR_INVALID_ARG`; exceptions are caught at the
C boundary and return `KOE_ERR_INTERNAL`. `KOE_OK` can still mean `q.valid == 0`.

## Measurements

- RMS is measured in complete 30 ms windows. The lower 10th percentile estimates the
  background level (`noise_db`).
- Windows above approximately 10 dB over the background and −56 dBFS are treated as voiced.
  `level_db` is the mean voiced-window RMS in dBFS; `speech_s` is their total duration.
- `koe_too_quiet` reports a valid voice level below −45 dBFS lasting at least 0.3 seconds.
- Input of at most four window lengths, or input without voiced windows, yields `valid = 0`.
- Additional fields report signal-to-noise ratio, clipping ratio, and brightness.

These are level-based heuristics, not speech recognition.

## Test data and fuzzing

This repository must not contain real recordings, audio files, or internal information.
Tests use synthetic waveforms only, including generated tones and noise.

CTest includes `quality`, `c_api` (C17 linkage and ABI layout), and `fuzz_smoke`. The smoke test feeds 300 deterministic
pseudo-random inputs to the fuzz target and runs in both normal and sanitizer builds.
For libFuzzer on macOS, install Homebrew LLVM (`brew install llvm`); Apple Clang does not include
libFuzzer. The build enables the `fuzz_measure` target only with that compiler:

```sh
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang
cmake --build build/fuzz --target fuzz_measure -j 2
mkdir -p build/fuzz/corpus
build/fuzz/fuzz_measure -max_total_time=600 -rss_limit_mb=2048 -max_len=65536 \
  build/fuzz/corpus fuzz/corpus
```

`fuzz/corpus/` holds a few small synthetic seeds (generated tones, silence, clipping, NaN/Inf
floats). The first input byte selects the sample rate, the second selects int16 or float32, and
the rest is PCM. `bin/ci` runs libFuzzer for 30 seconds when Homebrew LLVM is present and
skips it otherwise; the smoke test remains available without it.

## License

Copyright (c) 2026 chaspy. All rights reserved. No license is granted at this time.
See [LICENSE](LICENSE). This library is part of [Koe](https://github.com/chaspy/koe)
and follows the same terms.
