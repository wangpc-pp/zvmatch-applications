# Zvmatch Application Examples

This project demonstrates set-membership workloads for the experimental
RISC-V Zvmatch extension. Every workload has three interchangeable
implementations:

- scalar C reference;
- ordinary RVV using `vmseq.vx` and `vmor.mm`;
- RVV with `vmmatch.vx` or `vmmatch.vv`.

The driver validates fixed real-world examples and deterministic randomized
inputs. It deliberately uses 4099 elements so every vector implementation
executes partial final iterations at VLEN 128, 256, and 512.

## Workloads

| Workload | Element | Key set | Zvmatch form | Motivation |
|---|---:|---:|---|---|
| Hugging Face BERT ASCII boundary classification | 8 bit | 38 punctuation/whitespace bytes | `.vv` | Discover removable whitespace and isolated punctuation |
| Unicode whitespace classification | 16 bit | 25 BMP whitespace code points | `.vv` | `WhitespaceSplit` and BERT normalization fast path |
| CSV delimiter scan | 8 bit | comma, CR, LF, quote | `.vx` | Row, column, and quoted-field boundary candidates |
| JSON byte classification | 8 bit | three classes of at most 8 bytes | `.vx` | Structural, whitespace, and quote/escape masks |
| HTTP delimiter scan | 8 bit | 8 separators | `.vx` | Header/token boundary candidates |
| Database `IN`-list | 16 bit | 12 values | `.vv` | Selective column membership |
| Image palette membership | 8 bit | 16 values | `.vv` | Pixel classification against a small palette |
| Network port membership | 16 bit | 4 ports | `.vx` | Protocol metadata filtering |

The CSV workload includes comma, CR, LF, and quote candidates. A complete CSV
parser must still carry quote state before interpreting those candidates. The
JSON workload returns structural, whitespace, and quote/escape classes; it
does not implement quote-state propagation or the complete parser.

See `TOKENIZERS.md` for the source-backed tokenizer analysis.

## Why Both Instruction Forms Matter

`vmmatch.vx` packs `XLEN / SEW` keys into one scalar register. On RV64 that is
eight byte keys or four halfword keys, making it ideal for delimiters and
small protocol sets.

`vmmatch.vv` compares each query element with the complete VLMAX key register
group. It is used for larger character classes, database values, and palettes.
The instruction reads every key lane even when the active query VL is smaller.
The implementation therefore:

1. uses LMUL=4 so VLEN=128 already provides 64 byte or 32 halfword key lanes;
2. fills all VLMAX lanes with the first valid key;
3. performs a tail-undisturbed load of the distinct keys.

Repeating a valid key does not alter membership. Filling unused lanes with zero
would be incorrect because it would accidentally classify zero as a member.

## Per-Iteration Instruction Comparison

The table counts static instructions executed by one steady-state, non-final
outer loop iteration in the optimized RISC-V binaries. One iteration processes
one RVV chunk and includes:

- `vsetvli`, query load, membership operations, result materialization, and
  result store;
- count and pointer updates plus the taken outer-loop back edge;
- the fully expanded inner key loop used by ordinary RVV for variable-size
  key sets.

Function entry, one-time key loading or packing, the final `ret`, test-driver
code, and checksum code are excluded. Scalar C is omitted because its loop
processes one element rather than one VLEN-dependent vector chunk.

| Workload | Plain RVV | Zvmatch | Reduction |
|---|---:|---:|---:|
| Tokenizer ASCII boundaries | 198 | 10 | 94.9% |
| Tokenizer Unicode whitespace | 134 | 11 | 91.8% |
| CSV delimiter scan | 16 | 10 | 37.5% |
| JSON classification | 34 | 16 | 52.9% |
| HTTP delimiter scan | 24 | 10 | 58.3% |
| Database `IN`-list | 69 | 11 | 84.1% |
| Image palette membership | 88 | 10 | 88.6% |
| Network port membership | 17 | 11 | 35.3% |

The largest reductions occur for `.vv` workloads because ordinary RVV performs
one compare per key and combines the masks. The `.vx` workloads have smaller
sets, so Zvmatch replaces fewer compares. JSON uses three independent key
classes and therefore executes three `vmmatch.vx` instructions per iteration.

## Project Layout

- `applications.h`: common workload API;
- `keysets.c`: shared character/value sets;
- `reference.c`: scalar reference implementation;
- `rvv_without_zvmatch.c`: ordinary RVV comparison;
- `rvv_with_zvmatch.c`: Zvmatch implementation;
- `run.c`: deterministic validation driver;
- `cmake/riscv64-toolchain.cmake`: environment-driven cross-compilation;
- `scripts/run-qemu.sh`: multi-VLEN execution and feature-gate test;
- `scripts/count-instructions.sh`: QEMU plugin instruction profiling.

`run.c` is compiled with loop and SLP vectorization disabled. This keeps the
dynamic vector instruction counts focused on the explicit application kernels
rather than on checksum and verification loops.

## Prerequisites

Proof-of-concept software:

- LLVM: <https://github.com/wangpc-pp/llvm-project/tree/main-riscv-zvmatch>
- QEMU: <https://github.com/wangpc-pp/qemu/tree/master-zvmatch>

Set these environment variables:

| Variable | Required | Description |
|---|---|---|
| `RISCV_CLANG` | yes | Clang executable with Zvmatch intrinsic support |
| `RISCV_SYSROOT` | yes | RISC-V Linux sysroot |
| `RISCV_GCC_TOOLCHAIN` | optional | GCC toolchain root used for runtime objects and libraries |
| `QEMU_RISCV64` | for execution | `qemu-riscv64` with Zvmatch support |
| `QEMU_INSN_PLUGIN` | for counting | QEMU `libinsn.so` plugin |
| `BUILD_DIR` | optional | Build directory, default `build` |
| `BUILD_JOBS` | optional | Parallel build jobs, default `nproc` |
| `VLENS` | optional | Space-separated execution VLENs, default `128 256 512` |
| `VLEN` | optional | Profiling VLEN, default `128` |
| `QEMU_INSN_MATCHES` | optional | Instruction names counted by the plugin |

No source file or script contains a machine-specific tool path.

## Build and Run

```sh
export RISCV_CLANG=/path/to/clang
export RISCV_SYSROOT=/path/to/riscv/sysroot
export RISCV_GCC_TOOLCHAIN=/path/to/riscv/toolchain
export QEMU_RISCV64=/path/to/qemu-riscv64
export QEMU_INSN_PLUGIN=/path/to/libinsn.so

./run.sh
scripts/count-instructions.sh build
```

Static linking is enabled by default. It can be disabled with
`-DZVMATCH_APPLICATIONS_STATIC_LINK=OFF`.

The QEMU script runs:

- the scalar binary once;
- plain RVV and Zvmatch at VLEN 128, 256, and 512;
- a negative feature-gate test that requires the Zvmatch binary to fail when
  `zvmatch=false`.

## Verified Results

All eight workloads produce identical checksums for scalar C, ordinary RVV,
and Zvmatch at every tested VLEN:

```text
Tokenizer ASCII boundaries      9d8a7e813dfc7d14
Tokenizer Unicode whitespace    5eb4eb0d83e2bcea
CSV delimiter scan              0434eddb97af72fd
JSON classification             06737c4b315ab03c
HTTP delimiter scan             d245b891aa182d17
Database IN-list                28af85799169d04d
Image palette membership        58223d8054305944
Network port membership         4963f1c042fef028
```

At VLEN=128, the Zvmatch binary dynamically executes both forms:
`vmmatch.vv` for large sets and `vmmatch.vx` for packed small sets.

```text
variant        total-insns   vmmatch.vv   vmmatch.vx
scalar             5461692            0            0
rvv                3129410            0            0
zvmatch            3085630          388          454
```

The Zvmatch counts match the loop structure exactly. The four `.vv` workloads
execute 388 chunks in total; CSV, three JSON classes, HTTP, and network ports
execute 454 `.vx` operations.

The instruction totals are functional profiling data under QEMU TCG, not a
hardware performance claim. End-to-end speedups require measurements on real
implementations with representative hit rates and surrounding parser costs.
