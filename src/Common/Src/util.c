#include "util.h"

size_t util_strnlen(const char *s, size_t max_len) {
  size_t i = 0;
  while ((s[i] != '\0') && (i < max_len)) {
    i++;
  }
  return i;
}
