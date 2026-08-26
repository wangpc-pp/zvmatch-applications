#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

#include "applications.h"

const char *implementation_name(void) { return "RVV + Zvmatch (.vx only)"; }

__attribute__((always_inline)) static inline uintptr_t
pack_u8(const uint8_t k0, const uint8_t k1, const uint8_t k2, const uint8_t k3,
        const uint8_t k4, const uint8_t k5, const uint8_t k6,
        const uint8_t k7) {
  return (uintptr_t)k0 | ((uintptr_t)k1 << 8) | ((uintptr_t)k2 << 16) |
         ((uintptr_t)k3 << 24) | ((uintptr_t)k4 << 32) |
         ((uintptr_t)k5 << 40) | ((uintptr_t)k6 << 48) |
         ((uintptr_t)k7 << 56);
}

__attribute__((always_inline)) static inline uintptr_t
pack_u16(const uint16_t k0, const uint16_t k1, const uint16_t k2,
         const uint16_t k3) {
  return (uintptr_t)k0 | ((uintptr_t)k1 << 16) | ((uintptr_t)k2 << 32) |
         ((uintptr_t)k3 << 48);
}

__attribute__((always_inline)) static inline vuint8m4_t
materialize_u8(vbool2_t mask, uint8_t value, size_t vl, vuint8m4_t zero) {
  return __riscv_vmerge_vxm_u8m4(zero, value, mask, vl);
}

__attribute__((always_inline)) static inline vuint16m4_t
materialize_u16(vbool4_t mask, uint16_t value, size_t vl, vuint16m4_t zero) {
  return __riscv_vmerge_vxm_u16m4(zero, value, mask, vl);
}

__attribute__((always_inline)) static inline void
classify_u8_vx(const uint8_t *input, uint8_t *output, size_t count,
               const uintptr_t *packs, size_t num_packs) {
  size_t vlmax = __riscv_vsetvlmax_e8m4();
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t matches = __riscv_vmmatch_vx_u8m4_b2(values, packs[0], vl);
    for (size_t p = 1; p != num_packs; ++p) {
      vbool2_t next = __riscv_vmmatch_vx_u8m4_b2(values, packs[p], vl);
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
classify_u16_vx(const uint16_t *input, uint16_t *output, size_t count,
                const uintptr_t *packs, size_t num_packs) {
  size_t vlmax = __riscv_vsetvlmax_e16m4();
  vuint16m4_t zero = __riscv_vmv_v_x_u16m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e16m4(count);
    vuint16m4_t values = __riscv_vle16_v_u16m4(input, vl);
    vbool4_t matches = __riscv_vmmatch_vx_u16m4_b4(values, packs[0], vl);
    for (size_t p = 1; p != num_packs; ++p) {
      vbool4_t next = __riscv_vmmatch_vx_u16m4_b4(values, packs[p], vl);
      matches = __riscv_vmor_mm_b4(matches, next, vl);
    }
    vuint16m4_t result = materialize_u16(matches, UINT16_C(1), vl, zero);
    __riscv_vse16_v_u16m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void tokenizer_bert_ascii_boundaries(const uint8_t *input, uint8_t *output,
                                     size_t count) {
  const uintptr_t packs[5] = {
      pack_u8('\t', '\n', '\v', '\f', '\r', ' ', '!', '"'),
      pack_u8('#', '$', '%', '&', '\'', '(', ')', '*'),
      pack_u8('+', ',', '-', '.', '/', ':', ';', '<'),
      pack_u8('=', '>', '?', '@', '[', '\\', ']', '^'),
      pack_u8('_', '`', '{', '|', '}', '~', '\t', '\t'),
  };
  classify_u8_vx(input, output, count, packs, 5);
}

void tokenizer_unicode_whitespace(const uint16_t *input, uint16_t *output,
                                  size_t count) {
  const uintptr_t packs[7] = {
      pack_u16(0x0009, 0x000a, 0x000b, 0x000c),
      pack_u16(0x000d, 0x0020, 0x0085, 0x00a0),
      pack_u16(0x1680, 0x2000, 0x2001, 0x2002),
      pack_u16(0x2003, 0x2004, 0x2005, 0x2006),
      pack_u16(0x2007, 0x2008, 0x2009, 0x200a),
      pack_u16(0x2028, 0x2029, 0x202f, 0x205f),
      pack_u16(0x3000, 0x3000, 0x3000, 0x3000),
  };
  classify_u16_vx(input, output, count, packs, 7);
}

void csv_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  const uintptr_t packs[1] = {pack_u8(',', '\r', '\n', '"', ',', ',', ',', ',')};
  classify_u8_vx(input, output, count, packs, 1);
}

void json_classify(const uint8_t *input, uint8_t *output, size_t count) {
  uintptr_t structural = pack_u8('{', '}', '[', ']', ',', ':', '{', '{');
  uintptr_t whitespace = pack_u8(' ', '\t', '\r', '\n', ' ', ' ', ' ', ' ');
  uintptr_t quote_escape = pack_u8('"', '\\', '"', '"', '"', '"', '"', '"');
  size_t vlmax = __riscv_vsetvlmax_e8m4();
  vuint8m4_t zero = __riscv_vmv_v_x_u8m4(0, vlmax);
  while (count != 0) {
    size_t vl = __riscv_vsetvl_e8m4(count);
    vuint8m4_t values = __riscv_vle8_v_u8m4(input, vl);
    vbool2_t s = __riscv_vmmatch_vx_u8m4_b2(values, structural, vl);
    vbool2_t w = __riscv_vmmatch_vx_u8m4_b2(values, whitespace, vl);
    vbool2_t q = __riscv_vmmatch_vx_u8m4_b2(values, quote_escape, vl);
    vuint8m4_t result =
        materialize_u8(s, JSON_CLASS_STRUCTURAL, vl, zero);
    vuint8m4_t next = materialize_u8(w, JSON_CLASS_WHITESPACE, vl, zero);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    next = materialize_u8(q, JSON_CLASS_QUOTE_OR_ESCAPE, vl, zero);
    result = __riscv_vor_vv_u8m4(result, next, vl);
    __riscv_vse8_v_u8m4(output, result, vl);
    input += vl;
    output += vl;
    count -= vl;
  }
}

void http_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  const uintptr_t packs[1] = {
      pack_u8(' ', '\t', '\r', '\n', ':', ';', ',', '=')};
  classify_u8_vx(input, output, count, packs, 1);
}

void database_u16_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  const uintptr_t packs[3] = {
      pack_u16(7, 42, 99, 256),
      pack_u16(1024, 4096, 8191, 12345),
      pack_u16(16384, 32767, 49152, 65521),
  };
  classify_u16_vx(input, output, count, packs, 3);
}

void image_palette_membership(const uint8_t *input, uint8_t *output,
                              size_t count) {
  const uintptr_t packs[2] = {
      pack_u8(0x00, 0x12, 0x24, 0x36, 0x48, 0x5a, 0x6c, 0x7e),
      pack_u8(0x80, 0x92, 0xa4, 0xb6, 0xc8, 0xda, 0xec, 0xfe),
  };
  classify_u8_vx(input, output, count, packs, 2);
}

void network_port_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  const uintptr_t packs[1] = {pack_u16(22, 53, 80, 443)};
  classify_u16_vx(input, output, count, packs, 1);
}
