#!/usr/bin/env bash

set -euo pipefail
ulimit -c 0

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [build-directory]" >&2
  exit 2
fi

BUILD_DIR=${1:-${BUILD_DIR:-build}}
: "${QEMU_RISCV64:?Set QEMU_RISCV64 to the qemu-riscv64 executable}"

VLENS=${VLENS:-"128 256 512"}

if [[ ! -x ${QEMU_RISCV64} ]]; then
  echo "error: QEMU_RISCV64 is not executable: ${QEMU_RISCV64}" >&2
  exit 1
fi

run_scalar() {
  echo "=== Scalar C reference ==="
  "${QEMU_RISCV64}" -cpu "max,v=false,zvmatch=false" \
    "${BUILD_DIR}/run-scalar"
}

run_vector() {
  local vlen=$1
  local extension=$2
  local executable=$3
  local label=$4

  echo
  echo "=== VLEN=${vlen}: ${label} ==="
  "${QEMU_RISCV64}" \
    -cpu "max,v=true,vlen=${vlen},zvmatch=${extension}" \
    "${BUILD_DIR}/${executable}"
}

run_scalar

for vlen in ${VLENS}; do
  run_vector "${vlen}" false run-rvv "RVV without Zvmatch"
  run_vector "${vlen}" true run-zvmatch "RVV + Zvmatch (.vv or .vx)"
  run_vector "${vlen}" true run-zvmatch-vx "RVV + Zvmatch (.vx only)"
done

echo
echo "Checking that the Zvmatch binaries fail when Zvmatch is disabled"
negative_status=0
for binary in run-zvmatch run-zvmatch-vx; do
  bash -c '"$1" -cpu "max,v=true,vlen=128,zvmatch=false" "$2" >/dev/null 2>&1' \
    _ "${QEMU_RISCV64}" "${BUILD_DIR}/${binary}" 2>/dev/null ||
    negative_status=$?
  if [[ ${negative_status} -eq 0 ]]; then
    echo "error: ${binary} unexpectedly succeeded with zvmatch=false" >&2
    exit 1
  fi
done

echo "Negative feature-gate check passed (status=${negative_status})"
