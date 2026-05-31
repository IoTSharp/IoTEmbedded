#ifndef BASIC_DISPLAY_H
#define BASIC_DISPLAY_H

#include "Common/Inc/app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mb_interpreter_t;

#define BASIC_DISPLAY_FUNC_SCREEN "SCREEN"
#define BASIC_DISPLAY_FUNC_CLS    "CLS"
#define BASIC_DISPLAY_FUNC_COLOR  "COLOR"
#define BASIC_DISPLAY_FUNC_LOCATE "LOCATE"
#define BASIC_DISPLAY_FUNC_PSET   "PSET"
#define BASIC_DISPLAY_FUNC_PRESET "PRESET"
#define BASIC_DISPLAY_FUNC_LINE   "LINE"
#define BASIC_DISPLAY_FUNC_CIRCLE "CIRCLE"
#define BASIC_DISPLAY_FUNC_PAINT  "PAINT"

ErrorStatus basic_display_register(struct mb_interpreter_t *interpreter);
ErrorStatus basic_display_write_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif
