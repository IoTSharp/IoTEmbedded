#ifndef __UTIL_H
#define __UTIL_H

#include <stddef.h>

#define UTIL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum {
  UTIL_NORMAL = 0,
  UTIL_ABNORMAL = !UTIL_NORMAL
} util_service_state_t;

size_t util_strnlen(const char *s, size_t max_len);

#endif /* __UTIL_H */
