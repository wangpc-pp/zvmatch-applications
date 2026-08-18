#!/usr/bin/env bash

set -euo pipefail

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [build-directory]" >&2
  exit 2
fi

BUILD_DIR=${1:-${BUILD_DIR:-build}}
: "${QEMU_RISCV64:?Set QEMU_RISCV64 to the qemu-riscv64 executable}"
: "${QEMU_INSN_PLUGIN:?Set QEMU_INSN_PLUGIN to the QEMU libinsn.so plugin}"

VLEN=${VLEN:-128}
DEFAULT_MATCHES="vmmatch.vv vmmatch.vx vmseq vmor.mm vmerge vle8.v vle16.v vse8.v vse16.v vsetvli"
MATCHES=${QEMU_INSN_MATCHES:-${DEFAULT_MATCHES}}

if [[ ! -x ${QEMU_RISCV64} ]]; then
  echo "error: QEMU_RISCV64 is not executable: ${QEMU_RISCV64}" >&2
  exit 1
fi
if [[ ! -f ${QEMU_INSN_PLUGIN} ]]; then
  echo "error: QEMU_INSN_PLUGIN does not exist: ${QEMU_INSN_PLUGIN}" >&2
  exit 1
fi

declare -a names=("scalar" "rvv" "zvmatch")
declare -a executables=("run-scalar" "run-rvv" "run-zvmatch")
declare -a cpus=(
  "max,v=false,zvmatch=false"
  "max,v=true,vlen=${VLEN},zvmatch=false"
  "max,v=true,vlen=${VLEN},zvmatch=true"
)

match_arguments=""
for match in ${MATCHES}; do
  match_arguments+=",match=${match}"
done

printf "%-10s %15s" "variant" "total-insns"
for match in ${MATCHES}; do
  printf " %12s" "${match}"
done
printf "\n"

for index in "${!names[@]}"; do
  log_file=$(mktemp)
  trap 'rm -f "${log_file}"' EXIT

  plugin_arguments="inline=true"
  if [[ ${names[index]} != scalar ]]; then
    plugin_arguments+="${match_arguments}"
  fi

  "${QEMU_RISCV64}" \
    -cpu "${cpus[index]}" \
    -plugin "${QEMU_INSN_PLUGIN},${plugin_arguments}" \
    -d plugin \
    "${BUILD_DIR}/${executables[index]}" \
    >/dev/null 2>"${log_file}"

  total=$(awk '/^total insns:/ {print $3}' "${log_file}")
  if [[ -z ${total} ]]; then
    echo "error: QEMU instruction plugin did not report a total" >&2
    cat "${log_file}" >&2
    exit 1
  fi

  printf "%-10s %15s" "${names[index]}" "${total}"
  for match in ${MATCHES}; do
    hits=0
    if [[ ${names[index]} != scalar ]]; then
      hits=$(awk -v needle="${match}" '
        $1 == "Match:" {
          name = $2
          sub(/,$/, "", name)
          if (name == needle) {
            print $4
            found = 1
            exit
          }
        }
        END {
          if (!found)
            print 0
        }
      ' "${log_file}")
    fi
    printf " %12s" "${hits}"
  done
  printf "\n"

  rm -f "${log_file}"
  trap - EXIT
done
