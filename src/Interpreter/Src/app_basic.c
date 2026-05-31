#include "Interpreter/Inc/app_basic.h"

#include "Board/Inc/bsp_board.h"
#include "Common/Inc/log.h"
#include "Interpreter/Inc/app_basic_registry.h"
#include "Interpreter/Inc/basic.h"
#if BSP_HAS_DISPLAY
#include "Interpreter/Inc/basic_display.h"
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define APP_BASIC_SCRIPT_BUFFER_SIZE (EEPROM_BASIC_SCRIPT_MAX_SIZE + 1U)
#define APP_BASIC_IMPORT_BUFFER_SIZE (EEPROM_BASIC_SCRIPT_MAX_SIZE + 1U)
#define APP_BASIC_PRINT_BUFFER_SIZE  160U
#define APP_BASIC_DIRECT_STRING_FMT  "%s"

static struct mb_interpreter_t *app_basic_interpreter;
static bool app_basic_system_initialized;
static bool app_basic_import_in_progress;
static app_basic_print_target_t app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
static app_basic_status_t app_basic_status = {
  .loaded_slot = APP_BASIC_SLOT_PRIMARY,
};
static char app_basic_script_buffer[APP_BASIC_SCRIPT_BUFFER_SIZE];
static char app_basic_import_buffer[APP_BASIC_IMPORT_BUFFER_SIZE];

static ErrorStatus app_basic_load_from_slot(app_basic_slot_t slot);
static int app_basic_import_handler(struct mb_interpreter_t *s, const char *name);
static int app_basic_printer(struct mb_interpreter_t *s, const char *fmt, ...);
#if BSP_HAS_DISPLAY
static int app_basic_write_to_display(const char *fmt, va_list args);
static int app_basic_write_formatted_to_display(const char *fmt, va_list args);
#endif
static void app_basic_close_current(void);

void app_basic_init(void) {
  app_basic_close_current();
  if (!app_basic_system_initialized && mb_init() == MB_FUNC_OK) {
    app_basic_system_initialized = true;
  }
  memset(&app_basic_status, 0, sizeof(app_basic_status));
  app_basic_status.loaded_slot = APP_BASIC_SLOT_PRIMARY;
  app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
}

ErrorStatus app_basic_load(app_basic_slot_t preferred_slot) {
  if (app_basic_load_from_slot(preferred_slot) == SUCCESS) {
    return SUCCESS;
  }

  app_basic_slot_t fallback_slot =
    preferred_slot == APP_BASIC_SLOT_PRIMARY ? APP_BASIC_SLOT_BACKUP : APP_BASIC_SLOT_PRIMARY;
  if (app_basic_load_from_slot(fallback_slot) == SUCCESS) {
    LOG_WARNING("BASIC loaded fallback script %s", app_basic_status.loaded_name);
    return SUCCESS;
  }

  LOG_WARNING("BASIC script not loaded from EEPROM");
  return ERROR;
}

ErrorStatus app_basic_run_once(void) {
  if (app_basic_interpreter == NULL) {
    return ERROR;
  }

  int result = mb_run(app_basic_interpreter, true);
  if (result == MB_FUNC_OK || result == MB_FUNC_END) {
    return SUCCESS;
  }

  const char *file = NULL;
  int pos = 0;
  unsigned short row = 0U;
  unsigned short col = 0U;
  mb_error_e err = mb_get_last_error(app_basic_interpreter, &file, &pos, &row, &col);
  LOG_ERROR("BASIC run failed err=%d %s pos=%d row=%u col=%u", err, mb_get_error_desc(err), pos, row, col);
  return ERROR;
}

ErrorStatus app_basic_reload_and_run(app_basic_slot_t preferred_slot) {
  if (app_basic_load(preferred_slot) != SUCCESS) {
    return ERROR;
  }
  return app_basic_run_once();
}

app_basic_status_t app_basic_get_status(void) {
  return app_basic_status;
}

void app_basic_set_print_target(app_basic_print_target_t target) {
  app_basic_print_target = target;
}

app_basic_print_target_t app_basic_get_print_target(void) {
  return app_basic_print_target;
}

static ErrorStatus app_basic_load_from_slot(app_basic_slot_t slot) {
  eeprom_basic_script_slot_t eeprom_slot = (eeprom_basic_script_slot_t)slot;
  size_t script_size = 0U;
  if (!app_basic_system_initialized) {
    app_basic_init();
  }
  if (!app_basic_system_initialized) {
    return ERROR;
  }
  if (eeprom_read_basic_script(eeprom_slot, app_basic_script_buffer, sizeof(app_basic_script_buffer), &script_size) !=
      SUCCESS) {
    return ERROR;
  }

  app_basic_close_current();
  app_basic_print_target = APP_BASIC_PRINT_TARGET_DEFAULT;
  if (mb_open(&app_basic_interpreter) != MB_FUNC_OK || app_basic_interpreter == NULL) {
    app_basic_close_current();
    return ERROR;
  }

  (void)mb_set_printer(app_basic_interpreter, app_basic_printer);
  (void)mb_set_import_handler(app_basic_interpreter, app_basic_import_handler);
  if (app_basic_register_profile(app_basic_interpreter) != SUCCESS) {
    app_basic_close_current();
    return ERROR;
  }

  if (mb_load_string(app_basic_interpreter, app_basic_script_buffer, true) != MB_FUNC_OK) {
    app_basic_close_current();
    return ERROR;
  }

  memset(&app_basic_status, 0, sizeof(app_basic_status));
  app_basic_status.loaded_slot = slot;
  (void)strncpy(app_basic_status.loaded_name, eeprom_basic_script_slot_name(eeprom_slot),
                sizeof(app_basic_status.loaded_name) - 1U);
  app_basic_status.loaded_size = script_size;
  LOG_INFO("BASIC loaded %s size=%lu", app_basic_status.loaded_name, (uint32_t)script_size);
  return SUCCESS;
}

static int app_basic_import_handler(struct mb_interpreter_t *s, const char *name) {
  eeprom_basic_script_slot_t slot;
  size_t script_size = 0U;
  int result = MB_FUNC_ERR;
  if (app_basic_import_in_progress) {
    return MB_FUNC_ERR;
  }
  if (!eeprom_basic_script_slot_from_package_name(name, &slot)) {
    return MB_FUNC_ERR;
  }

  app_basic_import_in_progress = true;
  if (eeprom_read_basic_script(slot, app_basic_import_buffer, sizeof(app_basic_import_buffer), &script_size) != SUCCESS) {
    goto exit;
  }

  (void)script_size;
  result = mb_load_string(s, app_basic_import_buffer, true);

exit:
  app_basic_import_in_progress = false;
  return result;
}

static int app_basic_printer(struct mb_interpreter_t *s, const char *fmt, ...) {
  (void)s;
  if (fmt == NULL) {
    return 0;
  }

#if BSP_HAS_DISPLAY
  if (app_basic_print_target == APP_BASIC_PRINT_TARGET_DISPLAY) {
    va_list args;
    va_start(args, fmt);
    int result = app_basic_write_to_display(fmt, args);
    va_end(args);
    return result;
  }
#endif

  va_list args;
  va_start(args, fmt);
  int result = vprintf(fmt, args);
  va_end(args);
  return result;
}

#if BSP_HAS_DISPLAY
static int app_basic_write_to_display(const char *fmt, va_list args) {
  if (strcmp(fmt, APP_BASIC_DIRECT_STRING_FMT) == 0) {
    const char *text = va_arg(args, const char *);
    if (text == NULL) {
      text = "";
    }
    return basic_display_write_text(text) == SUCCESS ? (int)strlen(text) : -1;
  }
  return app_basic_write_formatted_to_display(fmt, args);
}

static int app_basic_write_formatted_to_display(const char *fmt, va_list args) {
  char buffer[APP_BASIC_PRINT_BUFFER_SIZE];
  int length = vsnprintf(buffer, sizeof(buffer), fmt, args);
  if (length < 0) {
    return length;
  }

  buffer[sizeof(buffer) - 1U] = '\0';
  if (basic_display_write_text(buffer) != SUCCESS) {
    return -1;
  }
  return length;
}
#endif

static void app_basic_close_current(void) {
  if (app_basic_interpreter != NULL) {
    (void)mb_close(&app_basic_interpreter);
    app_basic_interpreter = NULL;
  }
}
