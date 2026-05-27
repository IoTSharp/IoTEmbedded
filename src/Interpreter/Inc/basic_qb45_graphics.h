#ifndef BASIC_QB45_GRAPHICS_H
#define BASIC_QB45_GRAPHICS_H

#include "Common/Inc/app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mb_interpreter_t;

#define BASIC_QB45_FUNC_SCREEN "SCREEN"
#define BASIC_QB45_FUNC_CLS    "CLS"
#define BASIC_QB45_FUNC_COLOR  "COLOR"
#define BASIC_QB45_FUNC_LOCATE "LOCATE"
#define BASIC_QB45_FUNC_PSET   "PSET"
#define BASIC_QB45_FUNC_PRESET "PRESET"
#define BASIC_QB45_FUNC_LINE   "LINE"
#define BASIC_QB45_FUNC_CIRCLE "CIRCLE"
#define BASIC_QB45_FUNC_PAINT  "PAINT"

ErrorStatus basic_qb45_graphics_register(struct mb_interpreter_t *interpreter);
ErrorStatus basic_qb45_graphics_write_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif
