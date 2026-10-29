#pragma once

#include <stddef.h>

#define TF_NOT_FOUND ((size_t)-1)

/* Offset of the first match of `needle` in `text`, ignoring ASCII letter case (other bytes must
 * match exactly), or TF_NOT_FOUND. An empty needle never matches. */
size_t tf_find_nocase(const char *text, size_t len, const char *needle, size_t nlen);
