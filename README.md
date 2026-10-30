# Zvmatch Application Examples

This project demonstrates set-membership workloads for the experimental
RISC-V Zvmatch extension. Every workload has four interchangeable
implementations:

- scalar C reference;
- ordinary RVV using `vmseq.vx` and `vmor.mm`;
- RVV with Zvmatch, choosing `.vs` or `.vx` per workload (the recommended
  form);
- RVV with Zvmatch using only `.vx` for every workload (the alternative
  form, labeled `.vx only`).

The driver validates fixed real-world examples and deterministic randomized
inputs. It deliberately uses 4099 elements so every vector implementation
executes partial final iterations at VLEN 128, 256, 512, and 1024. A separate
`--short` mode checks the four `.vs` workloads at 24 input lengths from 0 to
513, including single-query matches against the last key group, zero values,
and untouched output beyond the requested count.

## Workloads

| Workload | Element | Key set | Zvmatch form | Motivation |
|---|---:|---:|---|---|
| Hugging Face BERT ASCII boundary classification | 8 bit | 38 punctuation/whitespace bytes | `.vs` | Discover removable whitespace and isolated punctuation |
| Unicode whitespace classification | 16 bit | 25 BMP whitespace code points | `.vs` | `WhitespaceSplit` and BERT normalization fast path |
| CSV delimiter scan | 8 bit | comma, CR, LF, quote | `.vx` | Row, column, and quoted-field boundary candidates |
| JSON byte classification | 8 bit | three classes of at most 8 bytes | `.vx` | Structural, whitespace, and quote/escape masks |
| HTTP delimiter scan | 8 bit | 8 separators | `.vx` | Header/token boundary candidates |
| Database `IN`-list | 16 bit | 12 values | `.vs` | Selective column membership |
| Image palette membership | 8 bit | 16 values | `.vs` | Pixel classification against a small palette |
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

`vmmatch.vs` compares each query element against the low 128 bits of a
separate scalar key register group: 16 byte keys or eight halfword keys.
That capacity is independent of query LMUL, VL, and VLEN. These applications
target VLEN >= 128 and therefore load each key group into one m1 register,
while using LMUL=4 for query batches.

The implementation initializes all 16 byte or eight halfword key slots,
fills unused positions by repeating the first valid key in that group, and
uses a tail-undisturbed load for the distinct keys. Repeating a valid key
does not alter membership. Filling unused slots with zero would incorrectly
classify zero for sets that exclude it.

Sets larger than 128 bits are split into fully initialized groups, with one
`vmmatch.vs` per group and `vmor.mm` to combine their masks:

| Workload | `.vs` key groups | Match and mask-OR operations per query chunk |
|---|---:|---|
| Tokenizer ASCII boundaries | 3 | 3 matches + 2 ORs |
| Tokenizer Unicode whitespace | 4 | 4 matches + 3 ORs |
| Database `IN`-list | 2 | 2 matches + 1 OR |
| Image palette membership | 1 | 1 match |

All key groups are loaded before the query loop and reused even when the
final query VL is smaller than the number of keys.

The earlier implementation loaded all 38 ASCII boundary keys into one e8m4
query-sized key group. Under the fixed 128-bit `.vs` semantics, only its
first 16 byte keys participate, so that implementation misses `?` in the
sample input at index 23. Splitting the logical set into fixed key groups
preserves every key with the current instruction definition.

## Per-Iteration Instruction Comparison

The table was regenerated from `llvm-objdump` of the validated binaries. It
counts static instructions executed by one steady-state, non-final
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

The `Zvmatch (.vx only)` column implements every workload with packed scalar-key
`.vx` operations, splitting keys larger than one register into multiple packs
combined with `vmor.mm`. The recommended `Zvmatch` binary instead uses `.vs`
for sets larger than one RV64 packed register, and `.vx` where a single
packed register already covers the set.

| Workload | Plain RVV | Zvmatch | Zvmatch (.vx only) | Reduction (Zvmatch) | Reduction (.vx only) |
|---|---:|---:|---:|---:|---:|
| Tokenizer ASCII boundaries | 198 | 13 | 17 | 93.4% | 91.4% |
| Tokenizer Unicode whitespace | 134 | 16 | 22 | 88.1% | 83.6% |
| CSV delimiter scan | 16 | 9 | 9 | 43.8% | 43.8% |
| JSON classification | 34 | 15 | 15 | 55.9% | 55.9% |
| HTTP delimiter scan | 24 | 9 | 9 | 62.5% | 62.5% |
| Database `IN`-list | 69 | 12 | 14 | 82.6% | 79.7% |
| Image palette membership | 88 | 9 | 11 | 89.8% | 87.5% |
| Network port membership | 17 | 10 | 10 | 41.2% | 41.2% |

For the larger sets, `.vs` covers twice as many keys per match as RV64
`.vx`, reducing the number of matches and mask ORs. Ordinary RVV still needs
one comparison per distinct key. The 38-byte tokenizer class uses three
`.vs` groups instead of five `.vx` packs; the 25-entry whitespace class uses
four groups instead of seven packs; the database list uses two groups
instead of three packs; and the palette uses one group instead of two packs.

For sets that fit in one scalar register, both forms need exactly one match
per class. The mixed implementation uses `.vx` for CSV, HTTP, network ports,
and the three JSON classes so their keys can remain in scalar registers.

## Project Layout

- `applications.h`: common workload API;
- `keysets.c`: shared character/value sets;
- `reference.c`: scalar reference implementation;
- `rvv_without_zvmatch.c`: ordinary RVV comparison;
- `rvv_with_zvmatch.c`: Zvmatch implementation, choosing `.vs` or `.vx`;
- `rvv_with_zvmatch_vx.c`: Zvmatch implementation using only `.vx`;
- `run.c`: deterministic validation driver and short-input boundary tests;
- `cmake/riscv64-toolchain.cmake`: environment-driven cross-compilation;
- `scripts/run-qemu.sh`: multi-VLEN execution and feature-gate test;
- `scripts/count-instructions.sh`: QEMU plugin instruction profiling.

`run.c` is compiled with loop and SLP vectorization disabled. This keeps the
dynamic vector instruction counts focused on the explicit application kernels
rather than on checksum and verification loops.

## Prerequisites

Proof-of-concept software:

- Specification: <https://github.com/wangpc-pp/riscv-isa-manual/tree/main-zvmatch>
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
| `VLENS` | optional | Space-separated execution VLENs, default `128 256 512 1024` |
| `VLEN` | optional | Profiling VLEN, default `128` |
| `QEMU_INSN_MATCHES` | optional | Instruction names counted by the plugin |

No source file or script contains a machine-specific tool path.

The validated QEMU snapshot builds its test plugins through Meson/Ninja.
Build the instruction plugin with the same QEMU source and build directory:

```sh
ninja -C /path/to/qemu/build tests/tcg/plugins/libinsn.so
export QEMU_INSN_PLUGIN=/path/to/qemu/build/tests/tcg/plugins/libinsn.so
```

This produces the API 7 plugin used by QEMU `11.0.93`.

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

- the scalar binary once for each validation mode;
- plain RVV, Zvmatch, and Zvmatch `.vx only` at VLEN 128, 256, 512, and 1024;
- default and `--short` validation for every implementation;
- a negative feature-gate test that requires the Zvmatch binaries to fail
  when `zvmatch=false`.

## Verified Results

These results were regenerated on 2026-10-30 from a fresh
`build-20261030-vs` directory using:

| Component | Validation snapshot |
|---|---|
| Zvmatch specification | `main-zvmatch` at `be8ddea03230206c477af2262b344fa9041a367f` |
| LLVM | Clang `24.0.0git`, `main-riscv-zvmatch` at `341fafa38fe07ba0c86ffddc13f0cf9cafcdb90f` plus local `.vs` updates; rebased onto upstream `0dc8e3c1e8e8` |
| QEMU | `11.0.93`, `master-zvmatch` at `6fc36e4fd34` plus local `.vs` updates |
| Linux runtime toolchain | `riscv64-unknown-linux-gnu-gcc` `15.2.1 20260319` and its sysroot |
| Application source | `main` at `1adf48513f2a145bed0651c3a039a0d7fd39f2b0` plus the application changes in this commit |
| Build | CMake Release, effective kernel optimization `-O2`, static linking, C11, `-Wall -Wextra -Werror` |
| ISA | Scalar `rv64gc`, RVV `rv64gcv`, Zvmatch `rv64gcv_zvmatch0p1`, `-menable-experimental-extensions` |
| Vector configuration | Query LMUL=4; VLEN 128, 256, 512, 1024; `.vs` keys loaded with LMUL=1 |

The driver disables loop and SLP vectorization with `-fno-vectorize` and
`-fno-slp-vectorize`. At validation time, LLVM and QEMU were built with local
changes implementing the fixed 128-bit `.vs` semantics. Those tested source
changes are now recorded in LLVM commit `07d206cdfc59` and QEMU commit
`e9828fcceacd`. The binary revision strings above identify their build-time
bases.

All 13 default runs passed (one scalar run plus three vector implementations
at four VLENs), as did all 13 separate short-input runs. The short-input mode
checks four kernels at 24 lengths per run, including count=0, count=1,
key-group and query-chunk boundaries, zero values, and output guards. Both
Zvmatch binaries failed with SIGILL (status 132) when `zvmatch=false`.

The default 4099-element workload produces identical checksums across scalar
C, ordinary RVV, Zvmatch, and Zvmatch `.vx only` for all eight kernels at
every tested VLEN:

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

At VLEN=128 and query LMUL=4, the default workload dynamically executes both
forms: `vmmatch.vs` for large sets and `vmmatch.vx` for packed small sets.
The separate `--short` validation runs are excluded from these counts.

The profiling script runs QEMU with an empty environment so guest startup
is independent of the host's environment variables. It profiles the complete
default process, including the runtime and validation driver. The scalar
reference executes 5433218 instructions. The vector measurements are:

| VLEN | Plain RVV total | Zvmatch total | `.vx only` total | Zvmatch `.vs` | Zvmatch `.vx` | `.vx only` `.vx` |
|---:|---:|---:|---:|---:|---:|---:|
| 128 | 3102117 | 3059002 | 3060261 | 1034 | 454 | 2199 |
| 256 | 3076503 | 3054803 | 3055351 | 522 | 230 | 1111 |
| 512 | 3063703 | 3052707 | 3052903 | 266 | 118 | 567 |
| 1024 | 3057303 | 3051659 | 3051679 | 138 | 62 | 295 |

The Zvmatch counts match the loop structure exactly. At VLEN=128, the four
`.vs` workloads execute 388 query chunks in total, using 1034 matches across
their key groups; CSV, three JSON classes, HTTP, and network ports execute 454 `.vx`
operations. The `.vx`-only binary performs 2199
`vmmatch.vx` operations: it splits the 38-byte tokenizer class into five
packs, the 25-entry Unicode whitespace into seven packs, the 12-value database
list into three packs, and the 16-byte palette into two packs.

At query LMUL=4, let `B=ceil(4099 / (VLEN / 2))` be the byte-query chunk
count and `H=ceil(4099 / (VLEN / 4))` the halfword-query chunk count. The
mixed binary executes `4*B + 6*H` `.vs` operations and `5*B + H` `.vx`
operations; the `.vx only` binary executes `12*B + 11*H` `.vx` operations.
These formulas agree with the plugin measurements at all four VLENs.

The instruction totals are functional profiling data under QEMU TCG, not a
hardware performance claim. End-to-end speedups require measurements on real
implementations with representative hit rates and surrounding parser costs.

To reproduce the validation and dynamic counts with the environment variables
above set to these toolchain builds:

```sh
export BUILD_DIR=build-20261030-vs
VLENS="128 256 512 1024" ./run.sh
for vlen in 128 256 512 1024; do
  VLEN="$vlen" scripts/count-instructions.sh "$BUILD_DIR"
done
```

Use `llvm-objdump` from the same LLVM build to inspect the static loops:

```sh
for binary in run-rvv run-zvmatch run-zvmatch-vx; do
  llvm-objdump -d --no-show-raw-insn "$BUILD_DIR/$binary" \
    > "$BUILD_DIR/$binary.asm"
done
```

Count the path from each workload's query `vsetvli` through its next outer
loop back edge. In the ordinary RVV kernels with a variable-size key set,
expand the inner five-instruction loop for each key after the first. The
entry setup and final return are excluded as described above.
