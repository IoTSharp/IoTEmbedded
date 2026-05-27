#include "Interpreter/Inc/basic_qb45_graphics.h"

#include "Common/Inc/log.h"
#include "Display/Inc/display_api.h"

ErrorStatus basic_qb45_graphics_register(struct mb_interpreter_t *interpreter) {
  const display_descriptor_t *display = NULL;

  if (interpreter == NULL) {
    return ERROR;
  }

  if (!display_api_is_available()) {
  return ERROR;
  }

  display = display_api_descriptor();
  if (display == NULL) {
    LOG_ERROR("BASIC QB4.5 graphics registration failed: display driver not bound");
    return ERROR;
  }

  LOG_INFO("BASIC QB4.5 graphics interface ready for %s controller=%s size=%ux%u", display->name,
           display->controller, (unsigned int)display->size.width, (unsigned int)display->size.height);
  LOG_INFO("BASIC QB4.5 builtins planned: %s %s %s %s %s %s %s %s %s", BASIC_QB45_FUNC_SCREEN, BASIC_QB45_FUNC_CLS,
           BASIC_QB45_FUNC_COLOR, BASIC_QB45_FUNC_LOCATE, BASIC_QB45_FUNC_PSET, BASIC_QB45_FUNC_PRESET,
           BASIC_QB45_FUNC_LINE, BASIC_QB45_FUNC_CIRCLE, BASIC_QB45_FUNC_PAINT);

  return SUCCESS;
}
