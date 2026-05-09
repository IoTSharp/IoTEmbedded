#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include "network_manager.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_SOCKET_MAX_COUNT 4U

/* 应用层只关心 TCP/UDP 语义，不直接依赖 CH395Q socket 号或 Air724UG AT 命令。 */
typedef enum {
  NETWORK_SOCKET_PROTO_UDP = 0,
  NETWORK_SOCKET_PROTO_TCP,
} network_socket_proto_t;

typedef struct {
  uint8_t socket_index;
  network_socket_proto_t proto;
  const char *remote_host;
  uint16_t remote_port;
  uint16_t local_port;
} network_socket_config_t;

typedef struct {
  network_link_t link;
  const char *name;
  bool (*is_ready)(void);
  bool (*open)(const network_socket_config_t *config);
  void (*close)(uint8_t socket_index);
  bool (*send)(uint8_t socket_index, const uint8_t *data, uint16_t length);
  bool (*send_to)(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data, uint16_t length);
  uint16_t (*recv)(uint8_t socket_index, uint8_t *data, uint16_t max_length);
  bool (*is_tcp_connected)(uint8_t socket_index);
} network_socket_ops_t;

bool network_socket_open(const network_socket_config_t *config);
void network_socket_close(uint8_t socket_index);
void network_socket_close_all(void);
bool network_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
bool network_socket_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data, uint16_t length);
uint16_t network_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
bool network_socket_is_tcp_connected(uint8_t socket_index);
bool network_socket_active_link_ready(void);
const char *network_socket_active_link_name(void);
bool network_socket_probe_tcp_port(const char *host, uint16_t port);

extern const network_socket_ops_t network_socket_ch395_ops;
extern const network_socket_ops_t network_socket_air724_ops;

#ifdef __cplusplus
}
#endif

#endif
