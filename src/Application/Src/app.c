#include "Application/Inc/app.h"

#include "Application/Inc/app_rtos.h"
#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#include "Bus/Rs485/Inc/bsp_rs485.h"
#include "Bus/Uart/Inc/bsp_uart.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Interpreter/Inc/app_basic.h"
#include "Network/Ch395/Inc/ch395_board.h"
#include "Network/Ch395/Inc/ch395_defs.h"
#include "Network/Ch395/Inc/ch395_driver.h"
#include "Config/Inc/config.h"
#include "Devices/Inc/device_manager.h"
#include "Common/Inc/log.h"
#include "Protocol/Modbus/Inc/modbus_api.h"
#include "Application/Inc/modbus_test.h"
#include "Protocol/Mqtt/Inc/mqtt_client.h"
#include "Network/Ap6181/Inc/bsp_ap6181.h"
#include "Network/Inc/network_manager.h"
#include "Protocol/Platform/Inc/platform_messages.h"
#if BSP_HAS_DISPLAY
#include "Display/Inc/display_api.h"
#endif

#include <stdio.h>

#ifndef APP_ENABLE_MODBUS_TEST_READ
#define APP_ENABLE_MODBUS_TEST_READ 0
#endif

#ifndef APP_ENABLE_FULL_STACK
#define APP_ENABLE_FULL_STACK 1
#endif

static void app_log_reset_cause(void);
#if BSP_HAS_DISPLAY
static void app_show_boot_screen(network_mode_t network_mode);
static void app_display_write_line(uint16_t row, const char *label, const char *value);
#endif

void app_init(void) {
#if BSP_HAS_DISPLAY
  bool display_ready = false;
#endif

  bsp_board_init();
  (void)bsp_watchdog_refresh();
  config_init();
  log_init(&active_config.log);
#if BSP_HAS_DISPLAY
  if (bsp_board_display_init() != SUCCESS) {
    LOG_WARNING("Board display binding init failed");
  } else {
    display_ready = true;
  }
#endif
  app_basic_init();

  network_mode_t network_mode = config_get_network_mode();
  const char *ch395_link_name = "CH395Q on UART4/CN2";

#if BSP_HAS_DISPLAY
  if (display_ready) {
    app_show_boot_screen(network_mode);
  }
#endif

  LOG_INFO("%s boot", BSP_BOARD_NAME);
  LOG_INFO("Firmware version: %s", CONFIG_VERSION);
  LOG_INFO("MCU: %s debug=%s", BSP_MCU_NAME, BSP_DEBUG_UART_NAME);
  LOG_INFO("SYSCLK: %lu Hz", HAL_RCC_GetSysClockFreq());
  app_log_reset_cause();

#if APP_ENABLE_FULL_STACK
  LOG_INFO("Network probe target: %s:%u every %lu ms", active_config.network_monitor.probe_host,
           active_config.network_monitor.probe_port, active_config.network_monitor.probe_interval_ms);
  LOG_INFO("Network mode: %s", config_network_mode_name(network_mode));
#if BSP_HAS_AP6181
  LOG_INFO("Network primary: AP6181 WiFi/SDMMC1");
  LOG_INFO("Network WiFi pins: %s", bsp_ap6181_pin_map());
  LOG_WARNING("AP6181 WiFi socket driver pending; MQTT must not use CH395Q/4G fallback on Pandora");
#elif BSP_HAS_CH395Q || BSP_HAS_AIR724UG
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
#else
  LOG_WARNING("Board network socket adapter not enabled");
#endif

  network_manager_init(&active_config.network_monitor, network_mode);
  network_manager_poll();
  /* 当前现场只验证本机 MQTT 闭环，NTP/UDP 不再参与启动路径，避免无意义等待挤占 IWDG 窗口。 */
  mqtt_client_init(&active_config.mqtt);
  platform_messages_init();
  device_manager_init();
  init_modbus();
  (void)bsp_rs485_start_receive_it();
#else
  LOG_INFO("Bring-up mode: full network/device stack disabled");
#endif
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
#if APP_ENABLE_FULL_STACK
  network_manager_poll();
  // MQTT 只依赖 network_socket 工厂接口，链路切换时由网络层关闭旧 socket 并在下一轮重连。
  (void)mqtt_client_maintain(now_ms, active_config.loop.keep_in_touch_with_server_interval);
  if (mqtt_client_is_ready()) {
    (void)platform_messages_request_register_info_as_needed(now_ms, active_config.loop.req_devices_reg_info_interval);
  }
  device_manager_poll(now_ms, active_config.loop.report_devices_data_interval);
#else
  (void)now_ms;
#endif
}

void app_loop(void) {
  app_process_once(HAL_GetTick());
  config_process_cmd();
  (void)bsp_watchdog_refresh();
  HAL_Delay(1000U);
}

bool network_prepare_ch395q_probe(void) {
#if BSP_HAS_CH395Q
  if (!bsp_ch395_is_reset_asserted() && ch395_cmd_check_exist(CH395_CHECK_TEST_DATA) == CH395_CHECK_EXPECTED) {
    return true;
  }

  /* 恢复探测前先释放 CH395Q 复位，再写入 IP/网关/掩码。
   * 这里保持单次初始化，避免每个探测周期把主业务拖成长阻塞。 */
  LOG_INFO("CH395Q prepare probe: release reset and init network params");
  return ch395_board_init_network(active_config.mqtt.local_ip, active_config.mqtt.gateway_ip, active_config.mqtt.mask_ip);
#else
  return false;
#endif
}

static void app_log_reset_cause(void) {
  const bool pin = __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET;
#if defined(RCC_FLAG_PORRST)
  const bool por = __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET;
#elif defined(RCC_FLAG_BORRST)
  const bool por = __HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET;
#else
  const bool por = false;
#endif
  const bool software = __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET;
  const bool iwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
  const bool wwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET;
  const bool low_power = __HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET;

  /* RCC 复位标志是判断现场“无日志重启”的第一证据；打印后立即清除，避免下次启动误判旧原因。 */
  LOG_INFO("reset.cause pin=%u por=%u soft=%u iwdg=%u wwdg=%u lpwr=%u", pin ? 1U : 0U, por ? 1U : 0U,
           software ? 1U : 0U, iwdg ? 1U : 0U, wwdg ? 1U : 0U, low_power ? 1U : 0U);
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

#if BSP_HAS_DISPLAY
static void app_show_boot_screen(network_mode_t network_mode) {
  const display_color_pair_t title_colors = {0x001FU, 0xFFFFU};
  const display_color_pair_t body_colors = {0x0000U, 0xFFFFU};
  char mqtt_target[40] = {0};
  char sysclk[24] = {0};

  (void)snprintf(mqtt_target, sizeof(mqtt_target), "%s:%u", active_config.mqtt.ip, active_config.mqtt.port);
  (void)snprintf(sysclk, sizeof(sysclk), "%luHZ", HAL_RCC_GetSysClockFreq());

  (void)display_api_cls(0xFFFFU);
  (void)display_api_color(title_colors);
  (void)display_api_locate((display_text_cursor_t){1U, 1U});
  (void)display_api_write_text("IOTEMBEDDED BOOT\n");

  (void)display_api_color(body_colors);
  app_display_write_line(3U, "BOARD", BSP_BOARD_NAME);
  app_display_write_line(4U, "FW", CONFIG_VERSION);
  app_display_write_line(5U, "MCU", BSP_MCU_NAME);
  app_display_write_line(6U, "SYSCLK", sysclk);
  app_display_write_line(7U, "NET", config_network_mode_name(network_mode));
  app_display_write_line(8U, "MQTT", mqtt_target);
#if BSP_HAS_AP6181
  app_display_write_line(9U, "LINK", "AP6181 WIFI");
#endif
  app_display_write_line(10U, "STATUS", "STARTING SERVICES");
}

static void app_display_write_line(uint16_t row, const char *label, const char *value) {
  char line[41] = {0};

  if (label == NULL || value == NULL) {
    return;
  }

  (void)snprintf(line, sizeof(line), "%-7s %s", label, value);
  (void)display_api_locate((display_text_cursor_t){row, 1U});
  (void)display_api_write_text(line);
}
#endif
