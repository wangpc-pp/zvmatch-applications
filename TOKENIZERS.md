# Hugging Face Tokenizers and Zvmatch

This analysis is based on Hugging Face `tokenizers` main commit
`447890f84deab25794ccfdee2a877901b6893569`.

## Conclusion

Zvmatch is relevant to tokenizer frontends as a character-class membership
primitive. It can produce masks for whitespace, punctuation, controls, and
other small Unicode classes before split-boundary construction. It does not
directly accelerate the BPE merge loop, vocabulary hashing, priority queues,
or offset bookkeeping.

The two executable tokenizer kernels are:

- `tokenizer_bert_ascii_boundaries`: an ASCII fast path that recognizes the
  six ASCII whitespace bytes and all 32 ASCII punctuation bytes with one
  `vmmatch.vv` per vector chunk;
- `tokenizer_unicode_whitespace`: a UTF-16/BMP membership pass covering the
  25 code points in the Unicode White_Space property with one
  `vmmatch.vv` per vector chunk.

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

For an ASCII byte stream, BERT's two relevant classes fit in one e8m4 key
register at the minimum VLEN:

- whitespace: TAB, LF, VT, FF, CR, and space;
- punctuation: `!` through `/`, `:` through `@`, `[` through `` ` ``, and
  `{` through `~`.

That is 38 distinct keys. At VLEN=128, e8m4 provides 64 key lanes. Ordinary
RVV requires 38 `vmseq.vx` operations and 37 mask ORs for each query chunk;
Zvmatch reduces membership testing to one `vmmatch.vv`.

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

There are 25 values. At VLEN=128, e16m4 supplies 32 key lanes, so one
`vmmatch.vv` covers the full class. All White_Space code points are in the BMP
and therefore need no surrogate-pair handling.

Rust's exact `char::is_whitespace` behavior is tied to its Unicode version.
An upstream tokenizer integration should generate this table from the same
Unicode data rather than treating the literal table here as permanently
authoritative.

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
