#include "parson_ex.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef va_copy
#define va_copy(dest, src) __va_copy(dest, src)
#endif

#define PARSON_EX_USEC_PER_SEC 1000000L
#define PARSON_EX_SEC_PER_DAY 86400LL

static size_t parson_ex_strnlen(const char *s, size_t max_len)
{
    size_t i = 0;

    if (s == NULL) {
        return 0;
    }

    while ((i < max_len) && (s[i] != '\0')) {
        i++;
    }

    return i;
}

static char *parson_ex_format_alloc(const char *format, va_list args, size_t min_len)
{
    char *buffer;
    size_t capacity = PARSON_EX_FORMAT_BUFFER_SIZE;
    int written;
    va_list args_copy;

    if (format == NULL) {
        return NULL;
    }

    if (capacity <= min_len) {
        capacity = min_len + 1u;
    }

    buffer = (char *)calloc(capacity, 1u);
    if (buffer == NULL) {
        return NULL;
    }

    va_copy(args_copy, args);
    written = vsnprintf(buffer, capacity, format, args_copy);
    va_end(args_copy);
    if (written < 0) {
        free(buffer);
        return NULL;
    }

    if ((size_t)written >= capacity) {
        size_t required = (size_t)written + 1u;
        char *larger;

        if (required <= min_len) {
            required = min_len + 1u;
        }

        larger = (char *)calloc(required, 1u);
        if (larger == NULL) {
            free(buffer);
            return NULL;
        }

        written = vsnprintf(larger, required, format, args);
        free(buffer);
        if ((written < 0) || ((size_t)written >= required)) {
            free(larger);
            return NULL;
        }
        buffer = larger;
    }

    return buffer;
}

JSON_Status json_object_set_string_args(JSON_Object *object, const char *name, const char *format, ...)
{
    char *buffer;
    JSON_Status result;
    va_list args;

    va_start(args, format);
    buffer = parson_ex_format_alloc(format, args, 0u);
    va_end(args);

    if (buffer == NULL) {
        return JSONFailure;
    }

    result = json_object_set_string(object, name, buffer);
    free(buffer);
    return result;
}

JSON_Status json_array_append_string_args(JSON_Array *array, const char *format, ...)
{
    char *buffer;
    JSON_Status result;
    va_list args;

    va_start(args, format);
    buffer = parson_ex_format_alloc(format, args, 0u);
    va_end(args);

    if (buffer == NULL) {
        return JSONFailure;
    }

    result = json_array_append_string(array, buffer);
    free(buffer);
    return result;
}

JSON_Status json_object_dotset_string_args(JSON_Object *object, const char *name, const char *format, ...)
{
    char *buffer;
    JSON_Status result;
    va_list args;

    va_start(args, format);
    buffer = parson_ex_format_alloc(format, args, 0u);
    va_end(args);

    if (buffer == NULL) {
        return JSONFailure;
    }

    result = json_object_dotset_string(object, name, buffer);
    free(buffer);
    return result;
}

JSON_Status json_object_dotset_string_args_with_len(JSON_Object *object, const char *name, size_t len, const char *format, ...)
{
    char *buffer;
    JSON_Status result;
    va_list args;

    va_start(args, format);
    buffer = parson_ex_format_alloc(format, args, len);
    va_end(args);

    if (buffer == NULL) {
        return JSONFailure;
    }

    result = json_object_dotset_string_with_len(object, name, buffer, len);
    free(buffer);
    return result;
}

JSON_Status json_object_dotset_string_ex(JSON_Object *object, const char *name, const char *string, int len)
{
    char *json_tmp;
    size_t copy_len;
    JSON_Status result;

    if ((string == NULL) || (len < 0)) {
        return JSONFailure;
    }

    copy_len = parson_ex_strnlen(string, (size_t)len);
    json_tmp = (char *)malloc(copy_len + 1u);
    if (json_tmp == NULL) {
        return JSONFailure;
    }

    memcpy(json_tmp, string, copy_len);
    json_tmp[copy_len] = '\0';
    result = json_object_dotset_string(object, name, json_tmp);
    free(json_tmp);
    return result;
}

JSON_Status json_object_set_string_ex(JSON_Object *object, const char *name, const char *string, int len)
{
    char *json_tmp;
    size_t copy_len;
    JSON_Status result;

    if ((string == NULL) || (len < 0)) {
        return JSONFailure;
    }

    copy_len = parson_ex_strnlen(string, (size_t)len);
    json_tmp = (char *)malloc(copy_len + 1u);
    if (json_tmp == NULL) {
        return JSONFailure;
    }

    memcpy(json_tmp, string, copy_len);
    json_tmp[copy_len] = '\0';
    result = json_object_set_string(object, name, json_tmp);
    free(json_tmp);
    return result;
}

static int parson_ex_is_leap_year(int year)
{
    return (((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0)));
}

static int parson_ex_days_in_month(int year, int month)
{
    static const int days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month < 1) || (month > 12)) {
        return 0;
    }

    if ((month == 2) && parson_ex_is_leap_year(year)) {
        return 29;
    }

    return days[month - 1];
}

static int64_t parson_ex_days_from_civil(int year, unsigned month, unsigned day)
{
    int era;
    unsigned yoe;
    unsigned doy;
    unsigned doe;

    year -= (month <= 2u) ? 1 : 0;
    era = (year >= 0) ? (year / 400) : ((year - 399) / 400);
    yoe = (unsigned)(year - (era * 400));
    doy = ((153u * (month + ((month > 2u) ? (unsigned)-3 : 9u))) + 2u) / 5u + day - 1u;
    doe = (yoe * 365u) + (yoe / 4u) - (yoe / 100u) + doy;

    return ((int64_t)era * 146097LL) + (int64_t)doe - 719468LL;
}

static void parson_ex_civil_from_days(int64_t days, int *year, unsigned *month, unsigned *day)
{
    int64_t era;
    unsigned doe;
    unsigned yoe;
    unsigned doy;
    unsigned mp;
    int y;

    days += 719468LL;
    era = (days >= 0) ? (days / 146097LL) : ((days - 146096LL) / 146097LL);
    doe = (unsigned)(days - (era * 146097LL));
    yoe = (doe - (doe / 1460u) + (doe / 36524u) - (doe / 146096u)) / 365u;
    y = (int)yoe + ((int)era * 400);
    doy = doe - ((365u * yoe) + (yoe / 4u) - (yoe / 100u));
    mp = ((5u * doy) + 2u) / 153u;

    *day = doy - (((153u * mp) + 2u) / 5u) + 1u;
    *month = mp + ((mp < 10u) ? 3u : (unsigned)-9);
    *year = y + ((*month <= 2u) ? 1 : 0);
}

static int parson_ex_digit(char ch)
{
    if ((ch < '0') || (ch > '9')) {
        return -1;
    }

    return ch - '0';
}

static int parson_ex_parse_2(const char *s)
{
    int tens = parson_ex_digit(s[0]);
    int ones = parson_ex_digit(s[1]);

    if ((tens < 0) || (ones < 0)) {
        return -1;
    }

    return (tens * 10) + ones;
}

static int parson_ex_parse_4(const char *s)
{
    int high = parson_ex_parse_2(s);
    int low = parson_ex_parse_2(s + 2);

    if ((high < 0) || (low < 0)) {
        return -1;
    }

    return (high * 100) + low;
}

static int parson_ex_parse_timezone(const char **p, int *offset_seconds)
{
    int sign;
    int hour;
    int minute = 0;

    if ((**p == '\0') || (**p == 'Z') || (**p == 'z')) {
        if ((**p == 'Z') || (**p == 'z')) {
            (*p)++;
        }
        *offset_seconds = 0;
        return 0;
    }

    if ((**p != '+') && (**p != '-')) {
        return -1;
    }

    sign = (**p == '+') ? 1 : -1;
    (*p)++;

    hour = parson_ex_parse_2(*p);
    if ((hour < 0) || (hour > 23)) {
        return -1;
    }
    *p += 2;

    if (**p == ':') {
        (*p)++;
        minute = parson_ex_parse_2(*p);
        if ((minute < 0) || (minute > 59)) {
            return -1;
        }
        *p += 2;
    } else if ((parson_ex_digit((*p)[0]) >= 0) && (parson_ex_digit((*p)[1]) >= 0)) {
        minute = parson_ex_parse_2(*p);
        if (minute > 59) {
            return -1;
        }
        *p += 2;
    }

    *offset_seconds = sign * ((hour * 3600) + (minute * 60));
    return 0;
}

static int parson_ex_parse_iso8601(const char *iso8601, int64_t *seconds, long *usec)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int offset_seconds;
    const char *p;

    if ((iso8601 == NULL) || (seconds == NULL) || (usec == NULL)) {
        return -1;
    }

    if (parson_ex_strnlen(iso8601, 20u) < 19u) {
        return -1;
    }

    year = parson_ex_parse_4(iso8601);
    month = parson_ex_parse_2(iso8601 + 5);
    day = parson_ex_parse_2(iso8601 + 8);
    hour = parson_ex_parse_2(iso8601 + 11);
    minute = parson_ex_parse_2(iso8601 + 14);
    second = parson_ex_parse_2(iso8601 + 17);

    if ((year < 0) || (month < 1) || (month > 12) ||
        (day < 1) || (day > parson_ex_days_in_month(year, month)) ||
        (hour < 0) || (hour > 23) ||
        (minute < 0) || (minute > 59) ||
        (second < 0) || (second > 59) ||
        (iso8601[4] != '-') || (iso8601[7] != '-') ||
        ((iso8601[10] != 'T') && (iso8601[10] != 't')) ||
        (iso8601[13] != ':') || (iso8601[16] != ':')) {
        return -1;
    }

    p = iso8601 + 19;
    *usec = 0;

    if (*p == '.') {
        int digits = 0;

        p++;
        if (parson_ex_digit(*p) < 0) {
            return -1;
        }

        while (parson_ex_digit(*p) >= 0) {
            if (digits < 6) {
                *usec = (*usec * 10L) + (long)parson_ex_digit(*p);
                digits++;
            }
            p++;
        }

        while (digits < 6) {
            *usec *= 10L;
            digits++;
        }
    }

    if (parson_ex_parse_timezone(&p, &offset_seconds) != 0) {
        return -1;
    }

    if (*p != '\0') {
        return -1;
    }

    *seconds = (parson_ex_days_from_civil(year, (unsigned)month, (unsigned)day) * PARSON_EX_SEC_PER_DAY) +
               ((int64_t)hour * 3600LL) +
               ((int64_t)minute * 60LL) +
               (int64_t)second -
               (int64_t)offset_seconds;

    return 0;
}

ssize_t format_timeval(struct timeval *tv, char *buf, size_t sz)
{
    int year;
    unsigned month;
    unsigned day;
    int64_t seconds;
    int64_t days;
    int64_t day_seconds;
    int hour;
    int minute;
    int second;
    long usec;
    int written;

    if ((tv == NULL) || (buf == NULL) || (sz == 0u)) {
        return -1;
    }

    seconds = (int64_t)tv->tv_sec;
    usec = (long)tv->tv_usec;
    if ((usec <= -PARSON_EX_USEC_PER_SEC) || (usec >= PARSON_EX_USEC_PER_SEC)) {
        seconds += (int64_t)(usec / PARSON_EX_USEC_PER_SEC);
        usec %= PARSON_EX_USEC_PER_SEC;
    }
    if (usec < 0) {
        usec += PARSON_EX_USEC_PER_SEC;
        seconds--;
    }

    days = seconds / PARSON_EX_SEC_PER_DAY;
    day_seconds = seconds % PARSON_EX_SEC_PER_DAY;
    if (day_seconds < 0) {
        day_seconds += PARSON_EX_SEC_PER_DAY;
        days--;
    }

    parson_ex_civil_from_days(days, &year, &month, &day);
    hour = (int)(day_seconds / 3600LL);
    day_seconds %= 3600LL;
    minute = (int)(day_seconds / 60LL);
    second = (int)(day_seconds % 60LL);

    written = snprintf(buf, sz, "%04d-%02u-%02uT%02d:%02d:%02d.%06ldZ",
                       year, month, day, hour, minute, second, usec);
    if ((written < 0) || ((size_t)written >= sz)) {
        return -1;
    }

    return (ssize_t)written;
}

JSON_Status json_object_dotset_string_iso8601(JSON_Object *object, const char *name, struct timeval *tv)
{
    char buffer[32];

    if (format_timeval(tv, buffer, sizeof(buffer)) <= 0) {
        return JSONFailure;
    }

    return json_object_dotset_string(object, name, buffer);
}

JSON_Status json_object_set_string_iso8601(JSON_Object *object, const char *name, struct timeval *tv)
{
    char buffer[32];

    if (format_timeval(tv, buffer, sizeof(buffer)) <= 0) {
        return JSONFailure;
    }

    return json_object_set_string(object, name, buffer);
}

JSON_Status json_object_dotset_time_t_iso8601(JSON_Object *object, const char *name, time_t timestamp)
{
    struct timeval tv;

    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    return json_object_dotset_string_iso8601(object, name, &tv);
}

JSON_Status json_object_set_time_t_iso8601(JSON_Object *object, const char *name, time_t timestamp)
{
    struct timeval tv;

    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    return json_object_set_string_iso8601(object, name, &tv);
}

time_t iso8601_to_time_t(const char *iso8601)
{
    int64_t seconds;
    long usec;

    if (parson_ex_parse_iso8601(iso8601, &seconds, &usec) != 0) {
        return (time_t)-1;
    }

    (void)usec;
    return (time_t)seconds;
}

int iso8601_to_timeval(const char *iso8601, struct timeval *tv)
{
    int64_t seconds;
    long usec;

    if ((tv == NULL) || (parson_ex_parse_iso8601(iso8601, &seconds, &usec) != 0)) {
        return -1;
    }

    tv->tv_sec = (time_t)seconds;
    tv->tv_usec = usec;
    return 0;
}
