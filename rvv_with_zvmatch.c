#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

#include "applications.h"

const char *implementation_name(void) { return "RVV + Zvmatch"; }

static uintptr_t pack_u8_keys(const uint8_t keys[8]) {
  uintptr_t packed = 0;
  for (size_t key = 0; key != 8; ++key)
    packed |= (uintptr_t)keys[key] << (key * 8);
  return packed;
}

static uintptr_t pack_u16_keys(const uint16_t keys[4]) {
  uintptr_t packed = 0;
  for (size_t key = 0; key != 4; ++key)
    packed |= (uintptr_t)keys[key] << (key * 16);
  return packed;
}

// Applications target VLEN >= 128, so one m1 register holds the scalar
// key group. Initialize every architectural key slot, even for a short VL.
static vuint8m1_t load_u8_keyset(const uint8_t *keys, size_t key_count) {
  vuint8m1_t result = __riscv_vmv_v_x_u8m1(keys[0], 16);
  return __riscv_vle8_v_u8m1_tu(result, keys, key_count < 16 ? key_count : 16);
}

static vuint16m1_t load_u16_keyset(const uint16_t *keys, size_t key_count) {
  vuint16m1_t result = __riscv_vmv_v_x_u16m1(keys[0], 8);
  return __riscv_vle16_v_u16m1_tu(result, keys, key_count < 8 ? key_count : 8);
}

static vuint8m4_t materialize_u8(vbool2_t mask, uint8_t value, size_t vl,
                                 vuint8m4_t zero) {
  return __riscv_vmerge_vxm_u8m4(zero, value, mask, vl);
}

static vuint16m4_t materialize_u16(vbool4_t mask, uint16_t value, size_t vl,
                                   vuint16m4_t zero) {
  return __riscv_vmerge_vxm_u16m4(zero, value, mask, vl);
}

__attribute__((always_inline)) static inline void
classify_u8_vs(const uint8_t *input, uint8_t *output, size_t count,
               const uint8_t *keys, size_t key_count, size_t key_group_count) {
  vuint8m1_t key0 = load_u8_keyset(keys, key_count);
  vuint8m1_t key1 = key0;
  vuint8m1_t key2 = key0;
  if (key_group_count > 1)
    key1 = load_u8_keyset(keys + 16, key_count - 16);
  if (key_group_count > 2)
    key2 = load_u8_keyset(keys + 32, key_count - 32);
  size_t vlmax = __riscv_vsetvlmax_e8m4();
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t matches = __riscv_vmmatch_vs_u8m4_b2(values, key0, vl);
    if (key_group_count > 1) {
      vbool2_t next = __riscv_vmmatch_vs_u8m4_b2(values, key1, vl);
      matches = __riscv_vmor_mm_b2(matches, next, vl);
    }
    if (key_group_count > 2) {
      vbool2_t next = __riscv_vmmatch_vs_u8m4_b2(values, key2, vl);
      matches = __riscv_vmor_mm_b2(matches, next, vl);
    }
    vuint8m4_t result = materialize_u8(matches, UINT8_C(1), vl, zero);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

__attribute__((always_inline)) static inline void
classify_u16_vs(const uint16_t *input, uint16_t *output, size_t count,
                const uint16_t *keys, size_t key_count,
                size_t key_group_count) {
  vuint16m1_t key0 = load_u16_keyset(keys, key_count);
  vuint16m1_t key1 = key0;
  vuint16m1_t key2 = key0;
  vuint16m1_t key3 = key0;
  if (key_group_count > 1)
    key1 = load_u16_keyset(keys + 8, key_count - 8);
  if (key_group_count > 2)
    key2 = load_u16_keyset(keys + 16, key_count - 16);
  if (key_group_count > 3)
    key3 = load_u16_keyset(keys + 24, key_count - 24);
  size_t vlmax = __riscv_vsetvlmax_e16m4();
  vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e16m4(count);
    vuint16m4_t values = __riscv_vle16_v_u16m4(input, vl);
    vbool4_t matches = __riscv_vmmatch_vs_u16m4_b4(values, key0, vl);
    if (key_group_count > 1) {
      vbool4_t next = __riscv_vmmatch_vs_u16m4_b4(values, key1, vl);
      matches = __riscv_vmor_mm_b4(matches, next, vl);
    }
    if (key_group_count > 2) {
      vbool4_t next = __riscv_vmmatch_vs_u16m4_b4(values, key2, vl);
      matches = __riscv_vmor_mm_b4(matches, next, vl);
    }
    if (key_group_count > 3) {
      vbool4_t next = __riscv_vmmatch_vs_u16m4_b4(values, key3, vl);
      matches = __riscv_vmor_mm_b4(matches, next, vl);
    }
    vuint16m4_t result = materialize_u16(matches, UINT16_C(1), vl, zero);
    __riscv_vse16_v_u16m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

static void classify_u8_vx(const uint8_t *input, uint8_t *output, size_t count,
                           const uint8_t keys[8]) {
  uintptr_t packed_keys = pack_u8_keys(keys);
  size_t vlmax = __riscv_vsetvlmax_e8m4();
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t matches = __riscv_vmmatch_vx_u8m4_b2(values, packed_keys, vl);
    vuint8m4_t result = materialize_u8(matches, UINT8_C(1), vl, zero);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

static void classify_u16_vx(const uint16_t *input, uint16_t *output,
                            size_t count, const uint16_t keys[4]) {
  uintptr_t packed_keys = pack_u16_keys(keys);
  size_t vlmax = __riscv_vsetvlmax_e16m4();
  vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e16m4(count);
    vuint16m4_t values = __riscv_vle16_v_u16m4(input, vl);
    vbool4_t matches = __riscv_vmmatch_vx_u16m4_b4(values, packed_keys, vl);
    vuint16m4_t result = materialize_u16(matches, UINT16_C(1), vl, zero);
    __riscv_vse16_v_u16m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void tokenizer_bert_ascii_boundaries(const uint8_t *input, uint8_t *output,
                                     size_t count) {
  classify_u8_vs(input, output, count, tokenizer_ascii_keys,
                 tokenizer_ascii_key_count, 3);
}

void tokenizer_unicode_whitespace(const uint16_t *input, uint16_t *output,
                                  size_t count) {
  classify_u16_vs(input, output, count, tokenizer_whitespace_keys,
                  tokenizer_whitespace_key_count, 4);
}

void csv_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  classify_u8_vx(input, output, count, csv_keys);
}

void json_classify(const uint8_t *input, uint8_t *output, size_t count) {
  uintptr_t structural_keys = pack_u8_keys(json_structural_keys);
  uintptr_t whitespace_keys = pack_u8_keys(json_whitespace_keys);
  uintptr_t quote_escape_keys = pack_u8_keys(json_quote_escape_keys);
  size_t vlmax = __riscv_vsetvlmax_e8m4();
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t structural =
        __riscv_vmmatch_vx_u8m4_b2(values, structural_keys, vl);
    vbool2_t whitespace =
        __riscv_vmmatch_vx_u8m4_b2(values, whitespace_keys, vl);
    vbool2_t quote_escape =
        __riscv_vmmatch_vx_u8m4_b2(values, quote_escape_keys, vl);
    vuint8m4_t result =
        materialize_u8(structural, JSON_CLASS_STRUCTURAL, vl, zero);
    vuint8m4_t next = materialize_u8(whitespace, JSON_CLASS_WHITESPACE, vl, zero);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    next = materialize_u8(quote_escape, JSON_CLASS_QUOTE_OR_ESCAPE, vl, zero);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void http_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  classify_u8_vx(input, output, count, http_delimiter_keys);
}

void database_u16_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  classify_u16_vs(input, output, count, database_keys, database_key_count, 2);
}

void image_palette_membership(const uint8_t *input, uint8_t *output,
                              size_t count) {
  classify_u8_vs(input, output, count, palette_keys, palette_key_count, 1);
}

void network_port_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  classify_u16_vx(input, output, count, network_port_keys);
}
