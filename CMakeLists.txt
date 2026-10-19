cmake_minimum_required(VERSION 3.20)
project(koe_audio_core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KOE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(KOE_BUILD_TESTS "Build tests" ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
if(KOE_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(koe_audio STATIC src/koe_audio.cpp)
target_include_directories(koe_audio PUBLIC include PRIVATE src)

if(KOE_BUILD_TESTS)
  enable_testing()
  add_executable(test_quality tests/test_quality.cpp)
  target_link_libraries(test_quality PRIVATE koe_audio)
  target_include_directories(test_quality PRIVATE src)
  add_test(NAME quality COMMAND test_quality)

  # libFuzzer is not in Apple clang. Use it only when Homebrew's llvm is present;
  # otherwise build a driver that feeds the same target pseudo-random inputs.
  set(KOE_LLVM_CLANGXX /opt/homebrew/opt/llvm/bin/clang++)
  if(CMAKE_CXX_COMPILER STREQUAL KOE_LLVM_CLANGXX)
    add_executable(fuzz_measure fuzz/fuzz_measure.cpp)
    target_compile_options(fuzz_measure PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_measure PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(fuzz_measure PRIVATE koe_audio)
  endif()
  add_executable(fuzz_smoke fuzz/fuzz_measure.cpp fuzz/smoke_main.cpp)
  target_link_libraries(fuzz_smoke PRIVATE koe_audio)
  add_test(NAME fuzz_smoke COMMAND fuzz_smoke)
endif()
