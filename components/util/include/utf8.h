#pragma once

#include <stddef.h>
#include <stdint.h>

/* Decodes one UTF-8 sequence from s (len > 0). Returns the bytes consumed (at least 1);
 * malformed or truncated input yields U+FFFD and consumes one byte. Returns 0 only if len == 0. */
size_t utf8_next(const char *s, size_t len, uint32_t *cp);

/* Encodes cp into out; returns the byte count (1..4). Invalid codepoints encode U+FFFD. */
size_t utf8_encode(uint32_t cp, char out[4]);
