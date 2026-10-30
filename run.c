#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "applications.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

enum { TEST_COUNT = 4099 };

static uint8_t byte_input[TEST_COUNT];
static uint8_t byte_output[TEST_COUNT];
static uint16_t half_input[TEST_COUNT];
static uint16_t half_output[TEST_COUNT];

static uint32_t next_random(uint32_t *state) {
  uint32_t value = *state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

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

static uint64_t checksum_u8(const uint8_t *values, size_t count) {
  uint64_t hash = UINT64_C(1469598103934665603);
  for (size_t index = 0; index != count; ++index) {
    hash ^= values[index];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static uint64_t checksum_u16(const uint16_t *values, size_t count) {
  uint64_t hash = UINT64_C(1469598103934665603);
  for (size_t index = 0; index != count; ++index) {
    hash ^= values[index];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static int verify_u8(const char *name, const uint8_t *keys, size_t key_count) {
  size_t matches = 0;
  for (size_t index = 0; index != TEST_COUNT; ++index) {
    uint8_t expected = contains_u8(byte_input[index], keys, key_count);
    if (byte_output[index] != expected) {
      fprintf(stderr, "%s mismatch at %zu: input=%u actual=%u expected=%u\n",
              name, index, byte_input[index], byte_output[index], expected);
      return 1;
    }
    matches += expected;
  }
  printf("  %-31s PASS, matches=%4zu, checksum=%016" PRIx64 "\n", name, matches,
         checksum_u8(byte_output, TEST_COUNT));
  return 0;
}

static int verify_u16(const char *name, const uint16_t *keys,
                      size_t key_count) {
  size_t matches = 0;
  for (size_t index = 0; index != TEST_COUNT; ++index) {
    uint16_t expected = contains_u16(half_input[index], keys, key_count);
    if (half_output[index] != expected) {
      fprintf(stderr, "%s mismatch at %zu: input=%u actual=%u expected=%u\n",
              name, index, half_input[index], half_output[index], expected);
      return 1;
    }
    matches += expected;
  }
  printf("  %-31s PASS, matches=%4zu, checksum=%016" PRIx64 "\n", name, matches,
         checksum_u16(half_output, TEST_COUNT));
  return 0;
}

static void fill_byte_input(uint32_t seed) {
  uint32_t state = seed;
  for (size_t index = 0; index != TEST_COUNT; ++index)
    byte_input[index] = (uint8_t)next_random(&state);
}

static void fill_half_input(uint32_t seed) {
  uint32_t state = seed;
  for (size_t index = 0; index != TEST_COUNT; ++index)
    half_input[index] = (uint16_t)next_random(&state);
}

static int run_tokenizer_ascii(void) {
  static const char sample[] = "Hey friend!\tHow are you?!?";

  fill_byte_input(UINT32_C(0x10203040));
  memcpy(byte_input, sample, sizeof(sample) - 1);
  for (size_t index = 97; index < TEST_COUNT;
       index += tokenizer_ascii_key_count)
    byte_input[index] = tokenizer_ascii_keys[index % tokenizer_ascii_key_count];

  tokenizer_bert_ascii_boundaries(byte_input, byte_output, TEST_COUNT);
  return verify_u8("Tokenizer ASCII boundaries", tokenizer_ascii_keys,
                   tokenizer_ascii_key_count);
}

static int run_tokenizer_whitespace(void) {
  static const uint16_t sample[] = {
      'H',
      'e',
      'l',
      'l',
      'o',
      UINT16_C(0x0020),
      UINT16_C(0x4e16),
      UINT16_C(0x754c),
      UINT16_C(0x3000),
      'R',
      'V',
      'V',
      UINT16_C(0x2028),
      'Z',
      'v',
      'm',
      'a',
      't',
      'c',
      'h',
  };

  fill_half_input(UINT32_C(0x55667788));
  memcpy(half_input, sample, sizeof(sample));
  for (size_t index = 83; index < TEST_COUNT;
       index += tokenizer_whitespace_key_count)
    half_input[index] =
        tokenizer_whitespace_keys[index % tokenizer_whitespace_key_count];

  tokenizer_unicode_whitespace(half_input, half_output, TEST_COUNT);
  return verify_u16("Tokenizer Unicode whitespace", tokenizer_whitespace_keys,
                    tokenizer_whitespace_key_count);
}

static int run_csv(void) {
  static const char sample[] =
      "name,language,quote\r\nAda,C,\"hello, vectors\"\r\n";

  fill_byte_input(UINT32_C(0xabcdef01));
  memcpy(byte_input, sample, sizeof(sample) - 1);
  for (size_t index = 101; index < TEST_COUNT; index += 53)
    byte_input[index] = csv_keys[index % 4];

  csv_delimiter_mask(byte_input, byte_output, TEST_COUNT);
  return verify_u8("CSV delimiter scan", csv_keys, 4);
}

static uint8_t json_expected(uint8_t value) {
  return (contains_u8(value, json_structural_keys, 6) ? JSON_CLASS_STRUCTURAL
                                                      : 0) |
         (contains_u8(value, json_whitespace_keys, 4) ? JSON_CLASS_WHITESPACE
                                                      : 0) |
         (contains_u8(value, json_quote_escape_keys, 2)
              ? JSON_CLASS_QUOTE_OR_ESCAPE
              : 0);
}

static int run_json(void) {
  static const char sample[] =
      "{\n  \"model\": \"Zvmatch\", \"keys\": [8, 16], \"ok\": true\n}\n";
  size_t class_counts[8] = {0};

  fill_byte_input(UINT32_C(0x31415926));
  memcpy(byte_input, sample, sizeof(sample) - 1);
  for (size_t index = 79; index < TEST_COUNT; index += 47)
    byte_input[index] = json_structural_keys[index % 6];

  json_classify(byte_input, byte_output, TEST_COUNT);
  for (size_t index = 0; index != TEST_COUNT; ++index) {
    uint8_t expected = json_expected(byte_input[index]);
    if (byte_output[index] != expected) {
      fprintf(stderr,
              "JSON classification mismatch at %zu: input=%u actual=%u "
              "expected=%u\n",
              index, byte_input[index], byte_output[index], expected);
      return 1;
    }
    ++class_counts[expected];
  }
  printf("  %-31s PASS, classified=%4zu, checksum=%016" PRIx64 "\n",
         "JSON classification", TEST_COUNT - class_counts[0],
         checksum_u8(byte_output, TEST_COUNT));
  return 0;
}

static int run_http(void) {
  static const char sample[] =
      "GET /models HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n\r\n";

  fill_byte_input(UINT32_C(0x76543210));
  memcpy(byte_input, sample, sizeof(sample) - 1);
  for (size_t index = 89; index < TEST_COUNT; index += 61)
    byte_input[index] = http_delimiter_keys[index % 8];

  http_delimiter_mask(byte_input, byte_output, TEST_COUNT);
  return verify_u8("HTTP delimiter scan", http_delimiter_keys, 8);
}

static int run_database(void) {
  fill_half_input(UINT32_C(0x0badcafe));
  memcpy(half_input, database_keys,
         database_key_count * sizeof(database_keys[0]));
  for (size_t index = 71; index < TEST_COUNT; index += 43)
    half_input[index] = database_keys[index % database_key_count];

  database_u16_membership(half_input, half_output, TEST_COUNT);
  return verify_u16("Database IN-list", database_keys, database_key_count);
}

static int run_palette(void) {
  fill_byte_input(UINT32_C(0xc001d00d));
  memcpy(byte_input, palette_keys, palette_key_count);
  for (size_t index = 67; index < TEST_COUNT; index += 41)
    byte_input[index] = palette_keys[index % palette_key_count];

  image_palette_membership(byte_input, byte_output, TEST_COUNT);
  return verify_u8("Image palette membership", palette_keys, palette_key_count);
}

static int run_network_ports(void) {
  fill_half_input(UINT32_C(0x13579bdf));
  memcpy(half_input, network_port_keys, sizeof(network_port_keys));
  for (size_t index = 73; index < TEST_COUNT; index += 59)
    half_input[index] = network_port_keys[index % 4];

  network_port_membership(half_input, half_output, TEST_COUNT);
  return verify_u16("Network port membership", network_port_keys, 4);
}

static const size_t short_counts[] = {
    0,  1,  3,  7,   8,   9,   15,  16,  17,  31,  32,  33,
    63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512, 513,
};

static int run_short_u8(const char *name,
                        void (*classify)(const uint8_t *, uint8_t *, size_t),
                        const uint8_t *keys, size_t key_count) {
  for (size_t index = 0; index != TEST_COUNT; ++index)
    byte_input[index] = index < 2 * key_count
                            ? (index % 2 ? 0 : keys[key_count - 1 - index / 2])
                            : (uint8_t)(index - 2 * key_count);
  for (size_t test = 0; test != ARRAY_SIZE(short_counts); ++test) {
    size_t count = short_counts[test];
    memset(byte_output, 0xa5, sizeof(byte_output));
    classify(byte_input, byte_output, count);
    for (size_t index = 0; index != TEST_COUNT; ++index) {
      uint8_t expected = index < count
                             ? contains_u8(byte_input[index], keys, key_count)
                             : UINT8_C(0xa5);
      if (byte_output[index] != expected) {
        fprintf(stderr,
                "%s short-input mismatch: count=%zu index=%zu "
                "actual=%u expected=%u\n",
                name, count, index, byte_output[index], expected);
        return 1;
      }
    }
  }
  printf("  %-31s PASS, %zu input lengths\n", name, ARRAY_SIZE(short_counts));
  return 0;
}

static int run_short_u16(const char *name,
                         void (*classify)(const uint16_t *, uint16_t *, size_t),
                         const uint16_t *keys, size_t key_count) {
  for (size_t index = 0; index != TEST_COUNT; ++index)
    half_input[index] = index < 2 * key_count
                            ? (index % 2 ? 0 : keys[key_count - 1 - index / 2])
                            : (uint16_t)(index - 2 * key_count);
  for (size_t test = 0; test != ARRAY_SIZE(short_counts); ++test) {
    size_t count = short_counts[test];
    memset(half_output, 0xa5, sizeof(half_output));
    classify(half_input, half_output, count);
    for (size_t index = 0; index != TEST_COUNT; ++index) {
      uint16_t expected = index < count
                              ? contains_u16(half_input[index], keys, key_count)
                              : UINT16_C(0xa5a5);
      if (half_output[index] != expected) {
        fprintf(stderr,
                "%s short-input mismatch: count=%zu index=%zu "
                "actual=%u expected=%u\n",
                name, count, index, half_output[index], expected);
        return 1;
      }
    }
  }
  printf("  %-31s PASS, %zu input lengths\n", name, ARRAY_SIZE(short_counts));
  return 0;
}

static int run_short_inputs(void) {
  printf("Running %s short-input tests\n", implementation_name());
  if (run_short_u8("Tokenizer ASCII boundaries",
                   tokenizer_bert_ascii_boundaries, tokenizer_ascii_keys,
                   tokenizer_ascii_key_count) ||
      run_short_u16("Tokenizer Unicode whitespace",
                    tokenizer_unicode_whitespace, tokenizer_whitespace_keys,
                    tokenizer_whitespace_key_count) ||
      run_short_u16("Database IN-list", database_u16_membership, database_keys,
                    database_key_count) ||
      run_short_u8("Image palette membership", image_palette_membership,
                   palette_keys, palette_key_count))
    return 1;
  puts("All short-input tests passed");
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--short") == 0)
    return run_short_inputs();
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--short]\n", argv[0]);
    return 2;
  }

  printf("Running %s application tests with %u elements\n",
         implementation_name(), TEST_COUNT);

  if (run_tokenizer_ascii() || run_tokenizer_whitespace() || run_csv() ||
      run_json() || run_http() || run_database() || run_palette() ||
      run_network_ports())
    return 1;

  puts("All application tests passed");
  return 0;
}
