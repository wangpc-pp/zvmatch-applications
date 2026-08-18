#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

#include "applications.h"

const char *implementation_name(void) { return "RVV without Zvmatch"; }

static vbool2_t match_u8_keys(vuint8m4_t values, const uint8_t *keys,
                              size_t key_count, size_t vl) {
  vbool2_t result = __riscv_vmseq_vx_u8m4_b2(values, keys[0], vl);
  for (size_t key = 1; key != key_count; ++key) {
    vbool2_t equal = __riscv_vmseq_vx_u8m4_b2(values, keys[key], vl);
    result = __riscv_vmor_mm_b2(result, equal, vl);
  }
  return result;
}

static vbool4_t match_u16_keys(vuint16m4_t values, const uint16_t *keys,
                               size_t key_count, size_t vl) {
  vbool4_t result = __riscv_vmseq_vx_u16m4_b4(values, keys[0], vl);
  for (size_t key = 1; key != key_count; ++key) {
    vbool4_t equal = __riscv_vmseq_vx_u16m4_b4(values, keys[key], vl);
    result = __riscv_vmor_mm_b4(result, equal, vl);
  }
  return result;
}

static vuint8m4_t materialize_u8(vbool2_t mask, uint8_t value, size_t vl) {
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vl);
  return __riscv_vmerge_vxm_u8m4(zero, value, mask, vl);
}

static vuint16m4_t materialize_u16(vbool4_t mask, uint16_t value, size_t vl) {
  vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vl);
  return __riscv_vmerge_vxm_u16m4(zero, value, mask, vl);
}

static void classify_u8(const uint8_t *input, uint8_t *output, size_t count,
                        const uint8_t *keys, size_t key_count) {
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t matches = match_u8_keys(values, keys, key_count, vl);
    vuint8m4_t result = materialize_u8(matches, UINT8_C(1), vl);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

static void classify_u16(const uint16_t *input, uint16_t *output, size_t count,
                         const uint16_t *keys, size_t key_count) {
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e16m4(count);
    vuint16m4_t values = __riscv_vle16_v_u16m4(input, vl);
    vbool4_t matches = match_u16_keys(values, keys, key_count, vl);
    vuint16m4_t result = materialize_u16(matches, UINT16_C(1), vl);
    __riscv_vse16_v_u16m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void tokenizer_bert_ascii_boundaries(const uint8_t *input, uint8_t *output,
                                     size_t count) {
  classify_u8(input, output, count, tokenizer_ascii_keys,
              tokenizer_ascii_key_count);
}

void tokenizer_unicode_whitespace(const uint16_t *input, uint16_t *output,
                                  size_t count) {
  classify_u16(input, output, count, tokenizer_whitespace_keys,
               tokenizer_whitespace_key_count);
}

void csv_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  classify_u8(input, output, count, csv_keys, 4);
}

void json_classify(const uint8_t *input, uint8_t *output, size_t count) {
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t structural = match_u8_keys(values, json_structural_keys, 6, vl);
    vbool2_t whitespace = match_u8_keys(values, json_whitespace_keys, 4, vl);
    vbool2_t quote_escape =
        match_u8_keys(values, json_quote_escape_keys, 2, vl);
    vuint8m4_t result = materialize_u8(structural, JSON_CLASS_STRUCTURAL, vl);
    vuint8m4_t next = materialize_u8(whitespace, JSON_CLASS_WHITESPACE, vl);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    next = materialize_u8(quote_escape, JSON_CLASS_QUOTE_OR_ESCAPE, vl);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void http_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  classify_u8(input, output, count, http_delimiter_keys, 8);
}

void database_u16_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  classify_u16(input, output, count, database_keys, database_key_count);
}

void image_palette_membership(const uint8_t *input, uint8_t *output,
                              size_t count) {
  classify_u8(input, output, count, palette_keys, palette_key_count);
}

void network_port_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  classify_u16(input, output, count, network_port_keys, 4);
}
