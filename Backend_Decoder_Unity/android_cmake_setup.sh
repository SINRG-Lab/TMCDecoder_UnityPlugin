#!/bin/bash

# ============================
# Android Build Script
# ============================

# 🛠️ Set paths to your Android NDK and CMake
export ANDROID_NDK="/Applications/Unity/Hub/Editor/6000.0.24f1/PlaybackEngines/AndroidPlayer/NDK"
export CMAKE_CXX_FLAGS="-fno-fast-math -ffp-contract=off -fno-associative-math -fno-math-errno"
export CMAKE_BIN="/opt/homebrew/bin/cmake"

# 📁 Build configuration
ABI=arm64-v8a
PLATFORM=android-21
BUILD_DIR=build_android

# 🧼 Clean build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 🔧 Run CMake with Android toolchain
"$CMAKE_BIN" -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM="$PLATFORM" \
  -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
  -G "Unix Makefiles"

# 🛠️ Build the shared library (parallelized)
"$CMAKE_BIN" --build "$BUILD_DIR" -- -j$(nproc)
