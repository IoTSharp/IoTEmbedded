#include "Network/Ch395/Inc/ch395_board.h"

#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#include "Board/Inc/bsp_watchdog.h"
#include "Network/Ch395/Inc/ch395_defs.h"
#include "Network/Ch395/Inc/ch395_driver.h"
#include "Network/Ch395/Inc/ch395_probe.h"
#include "Common/Inc/log.h"

static ch395_board_status_t board_status = {0};

static const uint8_t default_mac_addr[6] = {0x02U, 0x50U, 0x45U, 0x4DU, 0x00U, 0x01U};

void ch395_board_init_default(void) {
#if BSP_HAS_CH395Q
  ch395_driver_init();
  board_status.check_result = ch395_cmd_check_exist(CH395_CHECK_TEST_DATA);
  board_status.present = board_status.check_result == CH395_CHECK_EXPECTED;
  board_status.version = board_status.present ? ch395_cmd_get_ver() : 0U;
  board_status.phy_status = board_status.present ? ch395_cmd_get_phy_status() : 0U;
  board_status.init_status = CH395_ERR_UNKNOW;
#else
  board_status.present = false;
  board_status.check_result = 0U;
  board_status.version = 0U;
  board_status.phy_status = 0U;
  board_status.init_status = CH395_ERR_UNKNOW;
#endif
}

bool ch395_board_init_network(const char *local_ip, const char *gateway_ip, const char *mask_ip) {
#if BSP_HAS_CH395Q
  uint8_t local[4] = {0};
  uint8_t gateway[4] = {0};
  uint8_t mask[4] = {0};

  ch395_board_init_default();
  if (!board_status.present) {
    LOG_WARNING("CH395Q not detected, check UART4-to-CN2 jumper and RST isolation");
    return false;
  }

  if (!ch395_parse_ipv4(local_ip, local) || !ch395_parse_ipv4(gateway_ip, gateway) || !ch395_parse_ipv4(mask_ip, mask)) {
    LOG_WARNING("CH395Q network config invalid: ip=%s gw=%s mask=%s", local_ip == NULL ? "" : local_ip,
                gateway_ip == NULL ? "" : gateway_ip, mask_ip == NULL ? "" : mask_ip);
    return false;
  }

  ch395_cmd_reset();
  board_status.check_result = ch395_cmd_check_exist(CH395_CHECK_TEST_DATA);
  if (board_status.check_result != CH395_CHECK_EXPECTED) {
    board_status.present = false;
    board_status.init_status = CH395_ERR_UNKNOW;
    return false;
  }

  (void)ch395_set_mac_addr(default_mac_addr);
  (void)ch395_set_ip_addr(local);
  (void)ch395_set_gwip_addr(gateway);
  (void)ch395_set_mask_addr(mask);
  board_status.init_status = ch395_cmd_init();
  board_status.version = ch395_cmd_get_ver();
  board_status.phy_status = ch395_cmd_get_phy_status();
  board_status.present = true;

  return board_status.init_status == CH395_CMD_ERR_SUCCESS;
#else
  (void)local_ip;
  (void)gateway_ip;
  (void)mask_ip;
  ch395_board_init_default();
  return false;
#endif
}

ch395_board_status_t ch395_board_get_status(void) {
  if (bsp_ch395_is_reset_asserted()) {
    board_status.present = false;
    board_status.phy_status = 0U;
    return board_status;
  }

  if (board_status.present) {
    board_status.phy_status = ch395_cmd_get_phy_status();
  }
  return board_status;
}

void ch395_board_log_status(void) {
#if BSP_HAS_CH395Q
  ch395_board_status_t status = ch395_board_get_status();
  LOG_INFO("CH395Q check=0x%02X expected=0x%02X present=%u version=0x%02X phy=0x%02X init=0x%02X",
           status.check_result, CH395_CHECK_EXPECTED, status.present ? 1U : 0U, status.version, status.phy_status,
           status.init_status);
#else
  LOG_INFO("CH395Q unavailable on this board profile");
#endif
}
