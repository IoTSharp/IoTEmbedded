#include "Network/Ch395/Inc/ch395_driver.h"

#include "Modem/Inc/bsp_air724.h"
#include "Board/Inc/bsp_board.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#include "Bus/Uart/Inc/bsp_uart.h"
#include "Board/Inc/bsp_watchdog.h"

#include <stddef.h>

#define CH395_UART_SYNC1         0x57U
#define CH395_UART_SYNC2         0xABU
#define CH395_UART_TIMEOUT_MS    1000U
#define CH395_UART_RETRY_BAUDS   3U

static uint8_t ch395_write_data_bytes(const uint8_t *data, uint16_t length);
static uint8_t ch395_command_with_ip(uint8_t command, const uint8_t ip[4]);
static uint8_t ch395_command_with_socket(uint8_t command, uint8_t socket_index);
static bool ch395_uart_sync_to_work_baudrate(void);
static uint8_t ch395_uart_set_baudrate(uint32_t baudrate);
static HAL_StatusTypeDef ch395_uart_configure(uint32_t baudrate);

void ch395_driver_init(void) {
#if BSP_HAS_CH395Q
  /* 当前硬件只保留 UART4/CN2 访问 CH395Q。初始化时先压住 Air724UG，
   * 避免它在共享 UART4 上输出 URC 干扰 CH395Q 帧。 */
  bsp_air724_assert_reset();
  bsp_ch395_reset();
  if (!ch395_uart_sync_to_work_baudrate()) {
    (void)ch395_uart_configure(CH395_UART_WORK_BAUDRATE);
  }
#endif
}

void ch395_cmd_reset(void) {
#if BSP_HAS_CH395Q
  ch395_write_cmd(CH395_CMD_RESET_ALL);
  // CH395Q 软件复位后 UART 速率可能回到默认 9600，必须重新同步后再继续发网络配置。
  (void)bsp_watchdog_refresh();
  bsp_delay_ms(200U);
  (void)bsp_watchdog_refresh();
  (void)ch395_uart_sync_to_work_baudrate();
#endif
}

void ch395_write_cmd(uint8_t command) {
#if BSP_HAS_CH395Q
  const uint8_t header[] = {CH395_UART_SYNC1, CH395_UART_SYNC2, command};
  bsp_uart_flush_rx(BSP_UART4_HANDLE);
  (void)bsp_uart_write(BSP_UART4_HANDLE, header, (uint16_t)sizeof(header), CH395_UART_TIMEOUT_MS);
  // WCH UART/SPI 示例都要求命令字后保留短暂间隔，再继续写参数或读返回值。
  bsp_delay_us(2U);
#else
  (void)command;
#endif
}

HAL_StatusTypeDef ch395_write_data(uint8_t data) {
#if BSP_HAS_CH395Q
  return bsp_uart_write(BSP_UART4_HANDLE, &data, 1U, CH395_UART_TIMEOUT_MS);
#else
  (void)data;
  return HAL_ERROR;
#endif
}

HAL_StatusTypeDef ch395_read_data(uint8_t *data) {
  if (data == NULL) {
    return HAL_ERROR;
  }
#if BSP_HAS_CH395Q
  return bsp_uart_read(BSP_UART4_HANDLE, data, 1U, CH395_UART_TIMEOUT_MS);
#else
  return HAL_ERROR;
#endif
}

uint8_t ch395_cmd_get_ver(void) {
  uint8_t version = 0U;
  ch395_write_cmd(CH395_CMD_GET_IC_VER);
  (void)ch395_read_data(&version);
  return version;
}

uint8_t ch395_cmd_check_exist(uint8_t test_data) {
  uint8_t result = 0U;
  ch395_write_cmd(CH395_CMD_CHECK_EXIST);
  (void)ch395_write_data(test_data);
  (void)ch395_read_data(&result);
  return result;
}

bool ch395_check_exist(void) {
  return ch395_cmd_check_exist(CH395_CHECK_TEST_DATA) == CH395_CHECK_EXPECTED;
}

uint8_t ch395_cmd_init(void) {
  ch395_write_cmd(CH395_CMD_INIT_CH395);
  return ch395_wait_cmd_ready(2500U);
}

uint8_t ch395_get_cmd_status(void) {
  uint8_t status = CH395_ERR_UNKNOW;
  ch395_write_cmd(CH395_CMD_GET_CMD_STATUS);
  (void)ch395_read_data(&status);
  return status;
}

uint8_t ch395_cmd_get_phy_status(void) {
  uint8_t status = 0U;
  ch395_write_cmd(CH395_CMD_GET_PHY_STATUS);
  (void)ch395_read_data(&status);
  return status;
}

uint8_t ch395_get_socket_int(uint8_t socket_index) {
  uint8_t status = 0U;
  ch395_write_cmd(CH395_CMD_GET_SOCKET_INT_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_read_data(&status);
  return status;
}

uint8_t ch395_set_mac_addr(const uint8_t mac[6]) {
  ch395_write_cmd(CH395_CMD_SET_MAC_ADDR);
  uint8_t result = ch395_write_data_bytes(mac, 6U);
  bsp_delay_ms(100U);
  return result;
}

uint8_t ch395_set_ip_addr(const uint8_t ip[4]) {
  return ch395_command_with_ip(CH395_CMD_SET_IP_ADDR, ip);
}

uint8_t ch395_set_gwip_addr(const uint8_t ip[4]) {
  return ch395_command_with_ip(CH395_CMD_SET_GWIP_ADDR, ip);
}

uint8_t ch395_set_mask_addr(const uint8_t ip[4]) {
  return ch395_command_with_ip(CH395_CMD_SET_MASK_ADDR, ip);
}

uint8_t ch395_wait_cmd_ready(uint32_t timeout_ms) {
  uint32_t start = bsp_get_tick_ms();
  uint8_t status;

  do {
    /* CH395Q 初始化、断开和重连都可能阻塞到秒级；等待期间必须继续喂硬件 IWDG。 */
    (void)bsp_watchdog_refresh();
    bsp_delay_ms(5U);
    status = ch395_get_cmd_status();
    if (status != CH395_ERR_BUSY) {
      return status;
    }
  } while ((bsp_get_tick_ms() - start) < timeout_ms);

  return CH395_ERR_UNKNOW;
}

uint8_t ch395_set_socket_desip(uint8_t socket_index, const uint8_t ip[4]) {
  ch395_write_cmd(CH395_CMD_SET_IP_ADDR_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_write_data_bytes(ip, 4U);
  return CH395_CMD_ERR_SUCCESS;
}

uint8_t ch395_set_socket_desport(uint8_t socket_index, uint16_t port) {
  ch395_write_cmd(CH395_CMD_SET_DES_PORT_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_write_data((uint8_t)port);
  (void)ch395_write_data((uint8_t)(port >> 8));
  return CH395_CMD_ERR_SUCCESS;
}

uint8_t ch395_set_socket_sourport(uint8_t socket_index, uint16_t port) {
  ch395_write_cmd(CH395_CMD_SET_SOUR_PORT_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_write_data((uint8_t)port);
  (void)ch395_write_data((uint8_t)(port >> 8));
  return CH395_CMD_ERR_SUCCESS;
}

uint8_t ch395_set_socket_proto_type(uint8_t socket_index, uint8_t proto_type) {
  ch395_write_cmd(CH395_CMD_SET_PROTO_TYPE_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_write_data(proto_type);
  return CH395_CMD_ERR_SUCCESS;
}

uint8_t ch395_open_socket(uint8_t socket_index) {
  return ch395_command_with_socket(CH395_CMD_OPEN_SOCKET_SN, socket_index);
}

uint8_t ch395_tcp_connect(uint8_t socket_index) {
  return ch395_command_with_socket(CH395_CMD_TCP_CONNECT_SN, socket_index);
}

uint8_t ch395_tcp_disconnect(uint8_t socket_index) {
  return ch395_command_with_socket(CH395_CMD_TCP_DISCONNECT_SN, socket_index);
}

uint8_t ch395_close_socket(uint8_t socket_index) {
  return ch395_command_with_socket(CH395_CMD_CLOSE_SOCKET_SN, socket_index);
}

bool ch395_get_socket_tcp_state(uint8_t socket_index, uint8_t *tcp_state) {
  uint8_t open_state = 0U;
  uint8_t state = 0U;

  if (tcp_state == NULL) {
    return false;
  }

  ch395_write_cmd(CH395_CMD_GET_SOCKET_STATUS_SN);
  (void)ch395_write_data(socket_index);
  if (ch395_read_data(&open_state) != HAL_OK || ch395_read_data(&state) != HAL_OK) {
    return false;
  }

  *tcp_state = state;
  return true;
}

bool ch395_wait_socket_tcp_connected(uint8_t socket_index, uint32_t timeout_ms) {
  uint32_t start = bsp_get_tick_ms();
  uint8_t tcp_state = CH395_TCP_CLOSED;

  do {
    /* Broker 不可达或被防火墙拦截时，TCP 建连会走完整超时，不能让 IWDG 在这里复位。 */
    (void)bsp_watchdog_refresh();
    if (ch395_get_socket_tcp_state(socket_index, &tcp_state) && tcp_state == CH395_TCP_ESTABLISHED) {
      return true;
    }
    bsp_delay_ms(20U);
  } while ((bsp_get_tick_ms() - start) < timeout_ms);

  return false;
}

bool ch395_write_send_buf(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  if (data == NULL || length == 0U) {
    return false;
  }

  ch395_write_cmd(CH395_CMD_WRITE_SEND_BUF_SN);
  if (ch395_write_data(socket_index) != HAL_OK || ch395_write_data((uint8_t)length) != HAL_OK ||
      ch395_write_data((uint8_t)(length >> 8)) != HAL_OK) {
    return false;
  }
  return ch395_write_data_bytes(data, length) == CH395_CMD_ERR_SUCCESS;
}

uint16_t ch395_get_recv_length(uint8_t socket_index) {
  uint8_t low = 0U;
  uint8_t high = 0U;

  ch395_write_cmd(CH395_CMD_GET_RECV_LEN_SN);
  (void)ch395_write_data(socket_index);
  if (ch395_read_data(&low) != HAL_OK || ch395_read_data(&high) != HAL_OK) {
    return 0U;
  }

  return (uint16_t)(((uint16_t)high << 8) | low);
}

uint16_t ch395_read_recv_buf(uint8_t socket_index, uint8_t *data, uint16_t length) {
  if (data == NULL || length == 0U) {
    return 0U;
  }

#if BSP_HAS_CH395Q
  ch395_write_cmd(CH395_CMD_READ_RECV_BUF_SN);
  (void)ch395_write_data(socket_index);
  (void)ch395_write_data((uint8_t)length);
  (void)ch395_write_data((uint8_t)(length >> 8));

  return bsp_uart_read(BSP_UART4_HANDLE, data, length, CH395_UART_TIMEOUT_MS) == HAL_OK ? length : 0U;
#else
  (void)socket_index;
  return 0U;
#endif
}

void ch395_clear_recv_buf(uint8_t socket_index) {
  ch395_write_cmd(CH395_CMD_CLEAR_RECV_BUF_SN);
  (void)ch395_write_data(socket_index);
}

static uint8_t ch395_write_data_bytes(const uint8_t *data, uint16_t length) {
  if (data == NULL || length == 0U) {
    return CH395_ERR_UNKNOW;
  }
#if BSP_HAS_CH395Q
  return bsp_uart_write(BSP_UART4_HANDLE, data, length, CH395_UART_TIMEOUT_MS) == HAL_OK
           ? CH395_CMD_ERR_SUCCESS
           : CH395_ERR_UNKNOW;
#else
  return CH395_ERR_UNKNOW;
#endif
}

static uint8_t ch395_command_with_ip(uint8_t command, const uint8_t ip[4]) {
  ch395_write_cmd(command);
  return ch395_write_data_bytes(ip, 4U);
}

static uint8_t ch395_command_with_socket(uint8_t command, uint8_t socket_index) {
  ch395_write_cmd(command);
  (void)ch395_write_data(socket_index);
  return ch395_wait_cmd_ready(CH395_COMMAND_TIMEOUT_MS);
}

static bool ch395_uart_sync_to_work_baudrate(void) {
  static const uint32_t probe_bauds[CH395_UART_RETRY_BAUDS] = {
    CH395_UART_INIT_BAUDRATE,
    57600U,
    CH395_UART_WORK_BAUDRATE,
  };

  for (uint8_t i = 0U; i < CH395_UART_RETRY_BAUDS; i++) {
    if (ch395_uart_configure(probe_bauds[i]) != HAL_OK) {
      continue;
    }
    bsp_delay_ms(5U);

    uint8_t check = ch395_cmd_check_exist(CH395_CHECK_TEST_DATA);
    if (check != CH395_CHECK_EXPECTED) {
      continue;
    }

    if (probe_bauds[i] == CH395_UART_WORK_BAUDRATE) {
      return true;
    }

    uint8_t baud_status = ch395_uart_set_baudrate(CH395_UART_WORK_BAUDRATE);
    return baud_status == CH395_CMD_ERR_SUCCESS || ch395_cmd_check_exist(CH395_CHECK_TEST_DATA) == CH395_CHECK_EXPECTED;
  }

  return false;
}

static uint8_t ch395_uart_set_baudrate(uint32_t baudrate) {
  uint8_t result = CH395_ERR_UNKNOW;
  ch395_write_cmd(CH395_CMD_SET_BAUDRATE);
  (void)ch395_write_data((uint8_t)baudrate);
  (void)ch395_write_data((uint8_t)(baudrate >> 8));
  (void)ch395_write_data((uint8_t)(baudrate >> 16));

  /* WCH 例程在写完 3 字节速率后立刻切 MCU 侧 UART，再读取命令返回。
   * 这里把 CH395Q 与 Air724UG 都统一到 115200，便于后续链路切换时 UART4 不再频繁改速。 */
  bsp_delay_ms(1U);
  if (ch395_uart_configure(baudrate) == HAL_OK) {
    (void)ch395_read_data(&result);
  }
  return result;
}

static HAL_StatusTypeDef ch395_uart_configure(uint32_t baudrate) {
#if BSP_HAS_CH395Q
  return bsp_uart_configure_8n1(BSP_UART4_HANDLE, baudrate);
#else
  (void)baudrate;
  return HAL_ERROR;
#endif
}
