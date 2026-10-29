#include <ctype.h>

#include "textfind.h"

size_t tf_find_nocase(const char *text, size_t len, const char *needle, size_t nlen)
{
    if (nlen == 0 || nlen > len) {
        return TF_NOT_FOUND;
    }
    for (size_t i = 0; i + nlen <= len; i++) {
        size_t j = 0;
        while (j < nlen && tolower((unsigned char)text[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) {
            return i;
        }
    }
    return TF_NOT_FOUND;
}
