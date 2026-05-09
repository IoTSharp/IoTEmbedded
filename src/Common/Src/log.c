#include "Common/Inc/log.h"
#include <string.h>
#include <stdarg.h>

// 日志配置
static log_config_t *log_config;

// 日志级别字符串和枚举值
typedef struct {
  const char *str_value;
  log_level_t enum_value;
} log_level_item;

static const char *log_level_prefix_list[] = {"debug", "info", "warning", "error", "fatal", "cmd_rsp", "never"};

// 根据日志级别字符串获取枚举值
bool log_level_get_enum_value_by_str_value(const char *str_value, log_level_t *enum_value) {
  int i;
  for (i = 0; i < UTIL_ARRAY_SIZE(log_level_prefix_list); i++) {
    if (strcmp(str_value, log_level_prefix_list[i]) == 0) {
      *enum_value = (log_level_t)i;
      return true;
    }
  }
  return false;
}

// 根据日志级别枚举值获取字符串
bool log_level_get_str_value_by_enum_value(const char **str_value, log_level_t enum_value) {
  if (enum_value <= LOG_LEVEL_NEVER) {
    *str_value = log_level_prefix_list[enum_value];
    return true;
  }
  return false;
}

// 获取当前日志级别对应的字符串
const char *log_get_current_level_str_value(void) {
  if (log_config == NULL) {
    return log_level_prefix_list[LOG_LEVEL_NEVER];
  }
  return log_level_prefix_list[log_config->level];
}

// 日志初始化
void log_init(log_config_t *cfg) {
  log_config = cfg;
}

// 设置日志级别
void set_log_level(log_level_t level) {
  if (log_config == NULL) {
    return;
  }
  log_config->level = level;
}

// 获取日志级别
log_level_t get_log_level(void) {
  if (log_config == NULL) {
    return LOG_LEVEL_NEVER;
  }
  return log_config->level;
}

void log_as_needed(log_level_t level, const char *fmt, ...) {
  if (log_config == NULL || fmt == NULL) {
    return;
  }
  // 日志级别小于当前需要打印的日志级别，直接返回
  if (level < log_config->level) {
    return;
  }
  // 打印日志
  if (log_config->print_prefix) {
    printf("%s: ", log_level_prefix_list[level]);
  }
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  printf("\r\n");
}
