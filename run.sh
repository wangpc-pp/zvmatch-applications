#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "${SCRIPT_DIR}"

BUILD_DIR=${BUILD_DIR:-${SCRIPT_DIR}/build}

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/riscv64-toolchain.cmake" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS:-$(nproc)}"
"${SCRIPT_DIR}/scripts/run-qemu.sh" "${BUILD_DIR}"
