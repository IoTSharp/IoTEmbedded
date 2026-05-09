#ifndef CH395_SOCKET_H
#define CH395_SOCKET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CH395_SOCKET_PROTO_UDP = 2,
  CH395_SOCKET_PROTO_TCP = 3,
} ch395_socket_proto_t;

typedef struct {
  uint8_t socket_index;
  ch395_socket_proto_t proto;
  uint8_t remote_ip[4];
  uint16_t remote_port;
  uint16_t local_port;
} ch395_socket_config_t;

bool ch395_socket_open(const ch395_socket_config_t *config);
void ch395_socket_close(uint8_t socket_index);
bool ch395_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length);
bool ch395_socket_send_to(uint8_t socket_index, const uint8_t ip[4], uint16_t port, const uint8_t *data, uint16_t length);
uint16_t ch395_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length);
bool ch395_socket_is_tcp_connected(uint8_t socket_index);

#ifdef __cplusplus
}
#endif

#endif
