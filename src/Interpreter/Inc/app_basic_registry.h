#ifndef APP_BASIC_REGISTRY_H
#define APP_BASIC_REGISTRY_H

#include "Interpreter/Inc/app_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mb_interpreter_t;

typedef ErrorStatus (*app_basic_module_register_t)(struct mb_interpreter_t *interpreter);

typedef struct {
  const char *name;
  app_basic_module_register_t register_module;
  app_basic_capability_flags_t dependencies;
  app_basic_target_flags_t targets;
} app_basic_module_descriptor_t;

const app_basic_module_descriptor_t *app_basic_get_module_registry(size_t *count);
ErrorStatus app_basic_register_profile(struct mb_interpreter_t *interpreter);

#ifdef __cplusplus
}
#endif

#endif
