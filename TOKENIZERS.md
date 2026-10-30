# Hugging Face Tokenizers and Zvmatch

This analysis is based on Hugging Face `tokenizers` main commit
`447890f84deab25794ccfdee2a877901b6893569`. The executable kernels use the
2026-10-30 Zvmatch draft at riscv-isa-manual `main-zvmatch` commit
`be8ddea03230206c477af2262b344fa9041a367f`, where `.vs` has a fixed 128-bit
key group. See [README.md](README.md) for the regenerated instruction counts
and the compiler/QEMU validation configuration.

## Conclusion

Zvmatch is relevant to tokenizer frontends as a character-class membership
primitive. It can produce masks for whitespace, punctuation, controls, and
other small Unicode classes before split-boundary construction. It does not
directly accelerate the BPE merge loop, vocabulary hashing, priority queues,
or offset bookkeeping.

The two executable tokenizer kernels are:

- `tokenizer_bert_ascii_boundaries`: an ASCII fast path that recognizes the
  six ASCII whitespace bytes and all 32 ASCII punctuation bytes with three
  `vmmatch.vs` operations and two `vmor.mm` operations per vector chunk;
- `tokenizer_unicode_whitespace`: a UTF-16/BMP membership pass covering the
  25 code points in the Unicode White_Space property with four
  `vmmatch.vs` operations and three `vmor.mm` operations per vector chunk.

Both kernels return 0/1 classification arrays. A production integration would
consume the mask directly and preserve byte/character offset information
instead of materializing a byte or halfword per result.

## Source Mapping

`BertPreTokenizer` first removes whitespace delimiters and then isolates
punctuation:

- `tokenizers/src/pre_tokenizers/bert.rs:5`
- `tokenizers/src/pre_tokenizers/bert.rs:14`
- `tokenizers/src/pre_tokenizers/bert.rs:15`
- `tokenizers/src/pre_tokenizers/bert.rs:16`

The punctuation predicate explicitly combines ASCII punctuation with the
Unicode punctuation categories:

- `tokenizers/src/pre_tokenizers/bert.rs:6`
- `tokenizers/src/pre_tokenizers/punctuation.rs:7`
- `tokenizers/src/pre_tokenizers/punctuation.rs:8`

`WhitespaceSplit` delegates to Rust's Unicode-aware
`char::is_whitespace`:

- `tokenizers/src/pre_tokenizers/whitespace.rs:35`
- `tokenizers/src/pre_tokenizers/whitespace.rs:38`

The BERT normalizer separately identifies whitespace and control characters,
removes controls, and maps whitespace to ASCII space:

- `tokenizers/src/normalizers/bert.rs:7`
- `tokenizers/src/normalizers/bert.rs:16`
- `tokenizers/src/normalizers/bert.rs:92`
- `tokenizers/src/normalizers/bert.rs:94`
- `tokenizers/src/normalizers/bert.rs:95`

`ByteLevel` uses a regex to split the normalized input and then walks
`char_indices()` to map every UTF-8 byte into the GPT-2 byte alphabet:

- `tokenizers/src/pre_tokenizers/byte_level.rs:116`
- `tokenizers/src/pre_tokenizers/byte_level.rs:126`
- `tokenizers/src/pre_tokenizers/byte_level.rs:132`
- `tokenizers/src/pre_tokenizers/byte_level.rs:135`
- `tokenizers/src/pre_tokenizers/byte_level.rs:138`

Zvmatch does not replace the regex engine or UTF-8 decoding. It becomes useful
if the implementation introduces a bulk ASCII/UTF-16 classification stage
that feeds the existing split logic.

## BERT ASCII Fast Path

For an ASCII byte stream, BERT's two relevant classes are:

- whitespace: TAB, LF, VT, FF, CR, and space;
- punctuation: `!` through `/`, `:` through `@`, `[` through `` ` ``, and
  `{` through `~`.

That is 38 distinct keys. `vmmatch.vs` always uses 16 byte keys from the low
128 bits of its scalar key group, independent of query LMUL and VLEN. The
implementation loads three m1 key groups at VLEN >= 128, with 16, 16, and
six distinct keys; the last group repeats a valid key in its ten unused
slots. Ordinary RVV requires 38 comparisons and 37 mask ORs for each query
chunk; Zvmatch uses three matches and two mask ORs. Query batches use e8m4.

The result is a combined boundary-candidate mask. The caller still needs to
distinguish removal from isolation if it wants to reproduce
`BertPreTokenizer` exactly. That can be done with a separate six-key
whitespace match or by retaining the original bytes when consuming the mask.

## Unicode Whitespace Fast Path

The UTF-16 kernel covers the BMP code points commonly exposed by the Unicode
White_Space property:

```text
0009..000D 0020 0085 00A0 1680
2000..200A 2028 2029 202F 205F 3000
```

There are 25 values. Each `vmmatch.vs` reads eight halfword keys from a
fixed 128-bit scalar key group. The e16m4 query kernel therefore uses four
m1 key groups containing eight, eight, eight, and one distinct values,
combining four matches with three mask ORs. The final group duplicates
`0x3000` in all eight slots. All White_Space code points are in the BMP and
therefore need no surrogate-pair handling.

Rust's exact `char::is_whitespace` behavior is tied to its Unicode version.
An upstream tokenizer integration should generate this table from the same
Unicode data rather than treating the literal table here as permanently
authoritative.

## Measured Application Kernels

The 2026-10-30 validation uses Clang `24.0.0git` at `341fafa38fe0` plus local
`.vs` updates and QEMU `11.0.93` at `6fc36e4fd34` plus local `.vs` updates.
Both tokenizer kernels matched the scalar reference at VLEN 128, 256, 512,
and 1024, including 24 short-input lengths from 0 to 513.

The tested source changes are recorded in LLVM commit `07d206cdfc59` and
QEMU commit `e9828fcceacd`; the validation binaries retain the base revision
strings shown above.

The freshly built binaries execute the following static instruction counts
per steady-state query chunk, including loading queries, matching,
materializing and storing results, and loop control:

| Kernel | Plain RVV | Zvmatch `.vs` | Zvmatch `.vx only` | `.vs` reduction |
|---|---:|---:|---:|---:|
| ASCII boundary classification | 198 | 13 | 17 | 93.4% |
| Unicode whitespace classification | 134 | 16 | 22 | 88.1% |

These are instruction counts under the recorded compiler configuration.
Measurements of tokenizer throughput require a complete integration and
hardware execution. [README.md](README.md) records the full configuration,
dynamic counts, and reproduction commands.

## What Zvmatch Does Not Accelerate

The current Hugging Face implementation performs splitting over
`NormalizedString`, preserves original/normalized alignments, runs Unicode
category checks, and later executes model-specific tokenization. Zvmatch only
shortens regular membership tests. It does not directly help:

- regex matching in `Whitespace` or `ByteLevel`;
- non-ASCII general punctuation category lookup;
- UTF-8 validation and decoding;
- BPE pair hashing and ranked merge selection;
- Unigram lattice construction;
- generation and maintenance of normalized/original offsets.

The most credible integration path is therefore an ASCII fast path with a
fallback to existing Unicode logic, followed by profiling in complete
tokenizer pipelines.
