#ifndef ZVMATCH_APPLICATIONS_H
#define ZVMATCH_APPLICATIONS_H

#include <stddef.h>
#include <stdint.h>

enum {
  JSON_CLASS_STRUCTURAL = 1,
  JSON_CLASS_WHITESPACE = 2,
  JSON_CLASS_QUOTE_OR_ESCAPE = 4,
};

extern const uint8_t tokenizer_ascii_keys[];
extern const size_t tokenizer_ascii_key_count;
extern const uint16_t tokenizer_whitespace_keys[];
extern const size_t tokenizer_whitespace_key_count;
extern const uint8_t csv_keys[8];
extern const uint8_t json_structural_keys[8];
extern const uint8_t json_whitespace_keys[8];
extern const uint8_t json_quote_escape_keys[8];
extern const uint8_t http_delimiter_keys[8];
extern const uint16_t database_keys[];
extern const size_t database_key_count;
extern const uint8_t palette_keys[];
extern const size_t palette_key_count;
extern const uint16_t network_port_keys[4];

const char *implementation_name(void);

void tokenizer_bert_ascii_boundaries(const uint8_t *input, uint8_t *output,
                                     size_t count);
void tokenizer_unicode_whitespace(const uint16_t *input, uint16_t *output,
                                  size_t count);
void csv_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count);
void json_classify(const uint8_t *input, uint8_t *output, size_t count);
void http_delimiter_mask(const uint8_t *input, uint8_t *output, size_t count);
void database_u16_membership(const uint16_t *input, uint16_t *output,
                             size_t count);
void image_palette_membership(const uint8_t *input, uint8_t *output,
                              size_t count);
void network_port_membership(const uint16_t *input, uint16_t *output,
                             size_t count);

#endif
