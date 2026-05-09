#ifndef __LOG_H
#define __LOG_H

#include "util.h"
#include <stdio.h>
#include <stdbool.h>

// 日志级别
typedef enum {
  LOG_LEVEL_DEBUG,    // 调试日志
  LOG_LEVEL_INFO,     // 用户日志
  LOG_LEVEL_WARNING,  // 告警日志
  LOG_LEVEL_ERROR,    // 错误日志
  LOG_LEVEL_FATAL,    // 致命错误日志
  LOG_LEVEL_CMD_RESP, // 调试串口命令响应
  LOG_LEVEL_NEVER     // 不打印
} log_level_t;

// 日志配置
typedef struct {
  log_level_t level; // 日志级别（大于等于该级别的日志才会打印到调试串口）
  bool print_prefix; // 是否打印日志前缀，为UTIL_TRUE时在日志之前打印"debug: "、"info: "等信息
} log_config_t;

// 日志初始化
void log_init(log_config_t *cfg);
// 获取当前日志级别对应的字符串
const char *log_get_current_level_str_value(void);
// 根据日志级别字符串获取枚举值
bool log_level_get_enum_value_by_str_value(const char *str_value, log_level_t *enum_value);

// 按需记录日志
void log_as_needed(log_level_t level, const char *fmt, ...);
// 打印调试日志
#define LOG_DEBUG(fmt, ...)    log_as_needed(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
// 打印用户日志
#define LOG_INFO(fmt, ...)     log_as_needed(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
// 打印告警日志
#define LOG_WARNING(fmt, ...)  log_as_needed(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
// 打印错误日志
#define LOG_ERROR(fmt, ...)    log_as_needed(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
// 打印致命错误日志
#define LOG_FATAL(fmt, ...)    log_as_needed(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
// 打印调试串口命令响应日志
#define LOG_CMD_RESP(fmt, ...) log_as_needed(LOG_LEVEL_CMD_RESP, fmt, ##__VA_ARGS__)

#endif // __LOG_H
