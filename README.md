# koe-audio-core

koe（音声入力アプリ）の「声の大きさ・無音の判定」ライブラリ。C++20 で実装し、外向きは C の API（`include/koe_audio.h`）。
`macos/Sources/Koe/AudioQuality.swift`（koe-dev）の移植で、後で Swift から呼ぶ。private repo。

## 判定

- 30 ms の窓ごとの RMS を出し、下位 10% を背景（`noise_db`）とする
- 背景より 10 dB 以上大きく、かつ −56 dBFS 以上の窓を声とみなす。`level_db` は声の窓の平均、`speech_s` は声の長さ
- `koe_too_quiet`: `level_db < −45` かつ `speech_s >= 0.3`
- 窓が 4 つ以下の短い入力、声の窓がない入力は `valid = 0`
- Swift と同じ結果になる（`macos/Tests/quality/main.swift` の全ケースを `tests/test_quality.cpp` に移植。別に、決まった合成波形で Swift 版と全指標が一致することも確認した）

## ビルドとテスト

```sh
cmake -S . -B build/default && cmake --build build/default -j 2
ctest --test-dir build/default -j 2
bin/ci    # configure → build → ctest → ASan/UBSan 版の ctest → kagami check
```

ASan/UBSan だけ: `cmake -S . -B build/asan -DKOE_SANITIZE=ON`。テストは合成の正弦波と雑音だけを使う（録音は使わない）。

### ファジング

`fuzz/fuzz_measure.cpp` が libFuzzer のターゲット（任意のバイト列・サンプルレートで落ちないこと）。
Apple clang には libFuzzer が付いていないため、Homebrew の llvm（`/opt/homebrew/opt/llvm/bin/clang++`）で configure したときだけ `fuzz_measure` を作る:

```sh
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ && cmake --build build/fuzz --target fuzz_measure
build/fuzz/fuzz_measure -max_total_time=60
```

この環境には llvm が無いので libFuzzer は動かしていない。代わりに同じターゲットへ疑似乱数の入力 300 本を流す `fuzz_smoke`（ctest に含む）が通常ビルドと ASan/UBSan ビルドで走る。

## C API

```c
#include "koe_audio.h"

int16_t pcm[...];  // 16 kHz モノラル。呼び出し側が持つ
koe_quality q;
if (koe_measure_i16(pcm, n, 16000, &q) == KOE_OK && q.valid) {
  printf("%.1f dB, %.2f s\n", q.level_db, q.speech_s);
  if (koe_too_quiet(&q)) { /* 入力音量の案内を出す */ }
}
```

メモリの所有権は常に呼び出し側。ライブラリは `pcm` を呼び出し中に読むだけで、確保したメモリを返さない。
`float` 版（`koe_measure_f32`、±1.0 が満点。NaN は 0、範囲外は ±1 に丸める）もある。C++ の例外は境界で止めて `KOE_ERR_INTERNAL` にする。

## Windows 版（Go）との差

`windows/core/lowlevel.go` の `RecordingLevel` は、無音を切り落とした後の PCM 全体の RMS を出して −45 dBFS 未満かだけを返す。
背景推定・声の窓の選別・`speech_s`・0.3 秒条件はない（ミュート判定は別の `Peak`）。この ライブラリは macOS 版に合わせた。Go からの利用は後回し。

## C++ で書いて気づいたこと

- メモリ管理: `std::vector` と `std::span` だけで書けて、`new`/`delete` も生ポインタの所有も出てこない。C の境界（`koe_audio.cpp`）だけが生ポインタで、長さと NULL を最初に検査する。ASan ビルドで境界外アクセスは出なかった
- 未定義動作: 入力が `float` だと NaN が混ざり得る。NaN を `std::sort` に渡すと比較が strict weak ordering を壊して未定義動作になるので、取り込み時に 0 へ寄せた。`int16_t` の `abs(-32768)` も `int` に広げないと UB になる（Swift は `Int` に変換していた）。符号付き整数の暗黙変換は `-Wconversion` で拾った
- Swift との比較: Swift は配列の範囲外アクセスが必ずトラップし、整数オーバーフローも検出される。C++ は `operator[]` が検査なしなので、入力長の検査を自分で書き、その穴を sanitizer と fuzz で確かめる。Rust なら `unsafe` を書かない限り同じ穴は型で塞がる
- C++ の例外は C の境界を越えてはいけない。`catch (...)` で止めてエラーコードに変える必要がある（Swift / Rust では境界の外に出る失敗がそもそも型に現れる）
- kagami: 診断で出たのは「テストが無い」の 1 件のみで、メモリや UB の発見は sanitizer・コンパイラ警告側だった

## 次（この PR の範囲外）

- Swift からの呼び出し（モジュールマップ / SwiftPM）
- VAD・無音カット
- iOS
- Go（Windows 版）からの利用
