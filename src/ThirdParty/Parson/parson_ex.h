#ifndef parson_parson_ex_h
#define parson_parson_ex_h

#include <stddef.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "parson.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef PARSON_EX_FORMAT_BUFFER_SIZE
#define PARSON_EX_FORMAT_BUFFER_SIZE 256u
#endif

JSON_Status json_array_append_string_args(JSON_Array *array, const char *format, ...);
JSON_Status json_object_set_string_args(JSON_Object *object, const char *name, const char *format, ...);
JSON_Status json_object_dotset_string_args(JSON_Object *object, const char *name, const char *format, ...);
JSON_Status json_object_dotset_string_args_with_len(JSON_Object *object, const char *name, size_t len, const char *format, ...);
JSON_Status json_object_dotset_string_ex(JSON_Object *object, const char *name, const char *string, int len);
JSON_Status json_object_set_string_ex(JSON_Object *object, const char *name, const char *string, int len);
ssize_t format_timeval(struct timeval *tv, char *buf, size_t sz);
JSON_Status json_object_dotset_string_iso8601(JSON_Object *object, const char *name, struct timeval *tv);
JSON_Status json_object_set_string_iso8601(JSON_Object *object, const char *name, struct timeval *tv);
JSON_Status json_object_dotset_time_t_iso8601(JSON_Object *object, const char *name, time_t timestamp);
JSON_Status json_object_set_time_t_iso8601(JSON_Object *object, const char *name, time_t timestamp);
time_t iso8601_to_time_t(const char *iso8601);
int iso8601_to_timeval(const char *iso8601, struct timeval *tv);

#define json_object_dotset_string_struct(__object__, __name__, __string__) \
    json_object_dotset_string_ex((__object__), (__name__), (__string__), (int)sizeof(__string__))

#define json_object_set_string_struct(__object__, __name__, __string__) \
    json_object_set_string_ex((__object__), (__name__), (__string__), (int)sizeof(__string__))

#define json_object_dotset_char_struct(__object__, __name__, __char__) \
    json_object_dotset_string_args_with_len((__object__), (__name__), 1u, "%c", (__char__))

#define json_object_dotset_string_struct_iso8601(__object__, __name__, __structname_, __structmembername__) \
    json_object_dotset_time_t_iso8601((__object__), (__name__), (__structname_)->__structmembername__)

#define json_object_set_string_struct_iso8601(__object__, __name__, __structname_, __structmembername__) \
    json_object_set_time_t_iso8601((__object__), (__name__), (__structname_)->__structmembername__)

#ifdef __cplusplus
}
#endif

#endif
