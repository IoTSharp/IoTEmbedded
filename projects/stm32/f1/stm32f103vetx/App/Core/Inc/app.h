#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);
bool app_start_scheduler(void);
void app_process_once(uint32_t now_ms);
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif
