#include <stddef.h>
#include <stdint.h>

#include "applications.h"

const char *implementation_name(void) { return "scalar C reference"; }

static uint8_t contains_u8(uint8_t value, const uint8_t *keys,
                           size_t key_count) {
  for (size_t key = 0; key != key_count; ++key)
    if (value == keys[key])
      return UINT8_C(1);
  return UINT8_C(0);
}

static uint16_t contains_u16(uint16_t value, const uint16_t *keys,
                             size_t key_count) {
  for (size_t key = 0; key != key_count; ++key)
    if (value == keys[key])
      return UINT16_C(1);
  return UINT16_C(0);
}

void tokenizer_bert_ascii_boundaries(const uint8_t *input, uint8_t *output,
                                     size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u8(input[index], tokenizer_ascii_keys,
                                tokenizer_ascii_key_count);
}

void tokenizer_unicode_whitespace(const uint16_t *input, uint16_t *output,
                                  size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u16(input[index], tokenizer_whitespace_keys,
                                 tokenizer_whitespace_key_count);
}

void csv_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u8(input[index], csv_keys, 4);
}

void json_classify(const uint8_t *input, uint8_t *output, size_t count) {
  for (size_t index = 0; index != count; ++index) {
    uint8_t value = input[index];
    output[index] =
        (contains_u8(value, json_structural_keys, 6) ? JSON_CLASS_STRUCTURAL
                                                     : 0) |
        (contains_u8(value, json_whitespace_keys, 4) ? JSON_CLASS_WHITESPACE
                                                     : 0) |
        (contains_u8(value, json_quote_escape_keys, 2)
             ? JSON_CLASS_QUOTE_OR_ESCAPE
             : 0);
  }
}

void http_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u8(input[index], http_delimiter_keys, 8);
}

void database_u16_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] =
        contains_u16(input[index], database_keys, database_key_count);
}

void image_palette_membership(const uint8_t *input, uint8_t *output,
                              size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u8(input[index], palette_keys, palette_key_count);
}

void network_port_membership(const uint16_t *input, uint16_t *output,
                             size_t count) {
  for (size_t index = 0; index != count; ++index)
    output[index] = contains_u16(input[index], network_port_keys, 4);
}
