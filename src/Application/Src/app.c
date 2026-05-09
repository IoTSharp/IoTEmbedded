#include "Application/Inc/app.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Bus/Uart/Inc/bsp_uart.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Network/Ch395/Inc/ch395_board.h"
#include "Network/Ch395/Inc/ch395_defs.h"
#include "Network/Ch395/Inc/ch395_driver.h"
#include "Config/Inc/config.h"
#include "Devices/Inc/device_manager.h"
#include "Common/Inc/log.h"
#include "Protocol/Modbus/Inc/modbus_api.h"
#include "Application/Inc/modbus_test.h"
#include "Protocol/Mqtt/Inc/mqtt_client.h"
#include "Network/Inc/network_manager.h"
#include "Protocol/Platform/Inc/platform_messages.h"

#include <stdio.h>

#ifndef APP_ENABLE_MODBUS_TEST_READ
#define APP_ENABLE_MODBUS_TEST_READ 0
#endif

static void app_log_reset_cause(void);

void app_init(void) {
  bsp_board_init();
  (void)bsp_watchdog_refresh();
  config_init();
  log_init(&active_config.log);

  network_mode_t network_mode = config_get_network_mode();
  const char *ch395_link_name = "CH395Q on UART4/CN2";

  LOG_INFO("STM32F103VETX boot");
  LOG_INFO("Firmware version: %s", CONFIG_VERSION);
  LOG_INFO("SYSCLK: %lu Hz", HAL_RCC_GetSysClockFreq());
  app_log_reset_cause();
  LOG_INFO("Network probe target: %s:%u every %lu ms", active_config.network_monitor.probe_host,
           active_config.network_monitor.probe_port, active_config.network_monitor.probe_interval_ms);
  LOG_INFO("Network mode: %s", config_network_mode_name(network_mode));
  if (network_mode == NETWORK_MODE_AIR724UG) {
    LOG_INFO("Network primary: Air724UG 4G on UART4");
  } else if (network_mode == NETWORK_MODE_CH395Q) {
    LOG_INFO("Network primary: %s (fixed)", ch395_link_name);
  } else {
    LOG_INFO("Network primary: %s", ch395_link_name);
    LOG_INFO("Network fallback: Air724UG 4G on UART4");
  }
  LOG_INFO("Runtime mode: formal MQTT data reporting");

  if (network_mode != NETWORK_MODE_AIR724UG) {
    bool ch395_ready = ch395_board_init_network(active_config.mqtt.local_ip, active_config.mqtt.gateway_ip,
                                                active_config.mqtt.mask_ip);
    LOG_INFO("CH395Q init %s", ch395_ready ? "ok" : "failed");
    ch395_board_log_status();
  } else {
    /* 固定 4G 模式下不做 CH395Q 初始化，避免把启动时间浪费在未使用的链路上。 */
    LOG_INFO("CH395Q init skipped by network_mode=4g");
  }

  network_manager_init(&active_config.network_monitor, network_mode);
  network_manager_poll();
  /* 当前现场只验证本机 MQTT 闭环，NTP/UDP 不再参与启动路径，避免无意义等待挤占 IWDG 窗口。 */
  mqtt_client_init(&active_config.mqtt);
  platform_messages_init();
  device_manager_init();
  init_modbus();
  (void)bsp_rs485_start_receive_it();
  (void)bsp_debug_start_receive_it();
}

bool app_start_scheduler(void) {
  return app_rtos_start();
}

void app_process_once(uint32_t now_ms) {
#if APP_ENABLE_MODBUS_TEST_READ
  static uint8_t has_run_modbus_test = 0U;
  if (has_run_modbus_test == 0U) {
    has_run_modbus_test = 1U;
    (void)modbus_test_read_hold_register(1U, 0x0000U, 1U);
  }
#endif
  config_process_cmd();
  network_manager_poll();
  // MQTT 只依赖 network_socket 工厂接口，链路切换时由网络层关闭旧 socket 并在下一轮重连。
  (void)mqtt_client_maintain(now_ms, active_config.loop.keep_in_touch_with_server_interval);
  if (mqtt_client_is_ready()) {
    (void)platform_messages_request_register_info_as_needed(now_ms, active_config.loop.req_devices_reg_info_interval);
  }
  device_manager_poll(now_ms, active_config.loop.report_devices_data_interval);
}

void app_loop(void) {
  app_process_once(HAL_GetTick());
  (void)bsp_watchdog_refresh();
  HAL_Delay(1000U);
}

bool network_prepare_ch395q_probe(void) {
  if (!bsp_ch395_is_reset_asserted() && ch395_cmd_check_exist(CH395_CHECK_TEST_DATA) == CH395_CHECK_EXPECTED) {
    return true;
  }

  /* 恢复探测前先释放 CH395Q 复位，再写入 IP/网关/掩码。
   * 这里保持单次初始化，避免每个探测周期把主业务拖成长阻塞。 */
  LOG_INFO("CH395Q prepare probe: release reset and init network params");
  return ch395_board_init_network(active_config.mqtt.local_ip, active_config.mqtt.gateway_ip, active_config.mqtt.mask_ip);
}

static void app_log_reset_cause(void) {
  const bool pin = __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET;
  const bool por = __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET;
  const bool software = __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET;
  const bool iwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
  const bool wwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET;
  const bool low_power = __HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET;

  /* RCC 复位标志是判断现场“无日志重启”的第一证据；打印后立即清除，避免下次启动误判旧原因。 */
  LOG_INFO("reset.cause pin=%u por=%u soft=%u iwdg=%u wwdg=%u lpwr=%u", pin ? 1U : 0U, por ? 1U : 0U,
           software ? 1U : 0U, iwdg ? 1U : 0U, wwdg ? 1U : 0U, low_power ? 1U : 0U);
  __HAL_RCC_CLEAR_RESET_FLAGS();
}
