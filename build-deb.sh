#!/bin/bash
# Build the device .deb natively for aarch64 inside a Debian container.
# Colima on Apple Silicon runs arm64 Linux, so this is the real target arch --
# no cross toolchain and no BSP sysroot needed.
set -euo pipefail
cd "$(dirname "$0")"
docker run --rm -v "$PWD/app:/src" -w /src debian:trixie-slim bash -eux -c '
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends \
      build-essential cmake ninja-build pkg-config git ca-certificates \
      libpng-dev libjpeg62-turbo-dev libfreetype-dev zlib1g-dev libfmt-dev \
      dpkg-dev file >/dev/null

  rm -rf build/cm0-native
  # Building natively on aarch64, so the container root IS the sysroot the
  # CM0 branch is looking for -- no BSP download and no cross toolchain.
  cmake -S . -B build/cm0-native -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DUSE_DESKTOP=OFF \
      -DCM0_SDK_ROOT=/ \
      -DCM0_MULTIARCH=aarch64-linux-gnu \
      -DCM0_ALLOW_FETCH_DEPS=OFF
  cmake --build build/cm0-native -j"$(nproc)"
  cd build/cm0-native && cpack -G DEB
'
