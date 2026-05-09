#ifndef CH395_DRIVER_H
#define CH395_DRIVER_H

#include "ch395_defs.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ch395_driver_init(void);
void ch395_cmd_reset(void);
void ch395_write_cmd(uint8_t command);
HAL_StatusTypeDef ch395_write_data(uint8_t data);
HAL_StatusTypeDef ch395_read_data(uint8_t *data);
uint8_t ch395_cmd_get_ver(void);
uint8_t ch395_cmd_check_exist(uint8_t test_data);
bool ch395_check_exist(void);
uint8_t ch395_cmd_init(void);
uint8_t ch395_get_cmd_status(void);
uint8_t ch395_cmd_get_phy_status(void);
uint8_t ch395_get_socket_int(uint8_t socket_index);
uint8_t ch395_wait_cmd_ready(uint32_t timeout_ms);
uint8_t ch395_set_mac_addr(const uint8_t mac[6]);
uint8_t ch395_set_ip_addr(const uint8_t ip[4]);
uint8_t ch395_set_gwip_addr(const uint8_t ip[4]);
uint8_t ch395_set_mask_addr(const uint8_t ip[4]);
uint8_t ch395_set_socket_desip(uint8_t socket_index, const uint8_t ip[4]);
uint8_t ch395_set_socket_desport(uint8_t socket_index, uint16_t port);
uint8_t ch395_set_socket_sourport(uint8_t socket_index, uint16_t port);
uint8_t ch395_set_socket_proto_type(uint8_t socket_index, uint8_t proto_type);
uint8_t ch395_open_socket(uint8_t socket_index);
uint8_t ch395_tcp_connect(uint8_t socket_index);
uint8_t ch395_tcp_disconnect(uint8_t socket_index);
uint8_t ch395_close_socket(uint8_t socket_index);
bool ch395_get_socket_tcp_state(uint8_t socket_index, uint8_t *tcp_state);
// TCP 建连后要等 CH395Q 真正进入 ESTABLISHED，再把 socket 当成可用；否则容易把正常建连过程误判成失败。
bool ch395_wait_socket_tcp_connected(uint8_t socket_index, uint32_t timeout_ms);
bool ch395_write_send_buf(uint8_t socket_index, const uint8_t *data, uint16_t length);
uint16_t ch395_get_recv_length(uint8_t socket_index);
uint16_t ch395_read_recv_buf(uint8_t socket_index, uint8_t *data, uint16_t length);
void ch395_clear_recv_buf(uint8_t socket_index);

#ifdef __cplusplus
}
#endif

#endif
