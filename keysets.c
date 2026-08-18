#include <stddef.h>
#include <stdint.h>

#include "applications.h"

const uint8_t tokenizer_ascii_keys[] = {
    '\t', '\n', '\v', '\f', '\r', ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(',  ')',  '*',  '+',  ',',  '-', '.', '/', ':', ';', '<', '=', '>',
    '?',  '@',  '[',  '\\', ']',  '^', '_', '`', '{', '|', '}', '~',
};
const size_t tokenizer_ascii_key_count =
    sizeof(tokenizer_ascii_keys) / sizeof(tokenizer_ascii_keys[0]);

const uint16_t tokenizer_whitespace_keys[] = {
    UINT16_C(0x0009), UINT16_C(0x000a), UINT16_C(0x000b), UINT16_C(0x000c),
    UINT16_C(0x000d), UINT16_C(0x0020), UINT16_C(0x0085), UINT16_C(0x00a0),
    UINT16_C(0x1680), UINT16_C(0x2000), UINT16_C(0x2001), UINT16_C(0x2002),
    UINT16_C(0x2003), UINT16_C(0x2004), UINT16_C(0x2005), UINT16_C(0x2006),
    UINT16_C(0x2007), UINT16_C(0x2008), UINT16_C(0x2009), UINT16_C(0x200a),
    UINT16_C(0x2028), UINT16_C(0x2029), UINT16_C(0x202f), UINT16_C(0x205f),
    UINT16_C(0x3000),
};
const size_t tokenizer_whitespace_key_count =
    sizeof(tokenizer_whitespace_keys) / sizeof(tokenizer_whitespace_keys[0]);

const uint8_t csv_keys[8] = {',', '\r', '\n', '"', ',', ',', ',', ','};
const uint8_t json_structural_keys[8] = {'{', '}', '[', ']',
                                         ',', ':', '{', '{'};
const uint8_t json_whitespace_keys[8] = {' ', '\t', '\r', '\n',
                                         ' ', ' ',  ' ',  ' '};
const uint8_t json_quote_escape_keys[8] = {'"', '\\', '"', '"',
                                           '"', '"',  '"', '"'};
const uint8_t http_delimiter_keys[8] = {' ', '\t', '\r', '\n',
                                        ':', ';',  ',',  '='};

const uint16_t database_keys[] = {
    UINT16_C(7),     UINT16_C(42),    UINT16_C(99),    UINT16_C(256),
    UINT16_C(1024),  UINT16_C(4096),  UINT16_C(8191),  UINT16_C(12345),
    UINT16_C(16384), UINT16_C(32767), UINT16_C(49152), UINT16_C(65521),
};
const size_t database_key_count =
    sizeof(database_keys) / sizeof(database_keys[0]);

const uint8_t palette_keys[] = {
    UINT8_C(0x00), UINT8_C(0x12), UINT8_C(0x24), UINT8_C(0x36),
    UINT8_C(0x48), UINT8_C(0x5a), UINT8_C(0x6c), UINT8_C(0x7e),
    UINT8_C(0x80), UINT8_C(0x92), UINT8_C(0xa4), UINT8_C(0xb6),
    UINT8_C(0xc8), UINT8_C(0xda), UINT8_C(0xec), UINT8_C(0xfe),
};
const size_t palette_key_count = sizeof(palette_keys) / sizeof(palette_keys[0]);

const uint16_t network_port_keys[4] = {UINT16_C(22), UINT16_C(53), UINT16_C(80),
                                       UINT16_C(443)};
