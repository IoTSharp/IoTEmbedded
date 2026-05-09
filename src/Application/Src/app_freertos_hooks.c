#include "Board/Inc/bsp_watchdog.h"
#include "Common/Inc/log.h"

#include "FreeRTOS.h"
#include "task.h"

void vApplicationStackOverflowHook(TaskHandle_t task, signed char *task_name) {
  /* 栈溢出时不要继续喂 IWDG，让复位原因保持为 iwdg，同时尽量在串口留下任务名。 */
  LOG_FATAL("RTOS stack overflow task=%s", task_name == NULL ? "unknown" : (const char *)task_name);
  (void)task;
  taskDISABLE_INTERRUPTS();
  for (;;) {
  }
}

void vApplicationMallocFailedHook(void) {
  /* 线程/互斥量创建失败通常意味着 FreeRTOS heap 不够，停喂 IWDG 让现场能看到复位原因。 */
  LOG_FATAL("RTOS malloc failed");
  taskDISABLE_INTERRUPTS();
  for (;;) {
  }
}
