#ifndef AP6181_SOCKET_H
#define AP6181_SOCKET_H

#include "Config/Inc/network_config.h"
#include "Network/Inc/network_socket.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  AP6181_SOCKET_STATUS_DISABLED = 0,
  AP6181_SOCKET_STATUS_POWER_OFF,
  AP6181_SOCKET_STATUS_CONFIG_MISSING,
  AP6181_SOCKET_STATUS_BACKEND_MISSING,
  AP6181_SOCKET_STATUS_HARDWARE_INIT_FAILED,
  AP6181_SOCKET_STATUS_WIFI_JOIN_FAILED,
  AP6181_SOCKET_STATUS_IP_DOWN,
  AP6181_SOCKET_STATUS_SOCKET_ERROR,
  AP6181_SOCKET_STATUS_READY,
} ap6181_socket_status_t;

typedef struct {
  const char *name;
  bool (*init)(const network_wifi_config_t *config);
  bool (*is_ready)(void);
  bool (*open)(const network_socket_config_t *config);
  void (*close)(uint8_t socket_index);
  bool (*send)(uint8_t socket_index, const uint8_t *data, uint16_t length);
  bool (*send_to)(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data, uint16_t length);
  uint16_t (*recv)(uint8_t socket_index, uint8_t *data, uint16_t max_length);
  bool (*is_tcp_connected)(uint8_t socket_index);
} ap6181_socket_backend_ops_t;

void ap6181_socket_configure(const network_wifi_config_t *config);
bool ap6181_socket_register_backend(const ap6181_socket_backend_ops_t *ops);
bool ap6181_socket_is_ready(void);
bool ap6181_socket_open(const network_socket_config_t *config);
void ap6181_socket_close(uint8_t socket_index);
bool ap6181_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
bool ap6181_socket_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data,
                           uint16_t length);
uint16_t ap6181_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
bool ap6181_socket_is_tcp_connected(uint8_t socket_index);
void ap6181_socket_irq_notify(void);

ap6181_socket_status_t ap6181_socket_get_status(void);
const char *ap6181_socket_status_name(ap6181_socket_status_t status);
const char *ap6181_socket_status_detail(void);
const char *ap6181_socket_backend_name(void);
const char *ap6181_socket_configured_ssid(void);

#ifdef __cplusplus
}
#endif

#endif
