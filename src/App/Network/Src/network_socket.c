#include "network_socket.h"

#include "log.h"
#include <stddef.h>

typedef struct {
  const network_socket_ops_t *ops;
  bool opened;
} network_socket_owner_t;

static network_socket_owner_t socket_owners[NETWORK_SOCKET_MAX_COUNT];

static const network_socket_ops_t *network_socket_ops_for_link(network_link_t link);
static const network_socket_ops_t *network_socket_active_ops(void);
static const network_socket_ops_t *network_socket_owner_ops(uint8_t socket_index);
static bool network_socket_index_valid(uint8_t socket_index);

bool network_socket_open(const network_socket_config_t *config) {
  if (config == NULL || !network_socket_index_valid(config->socket_index) || config->remote_host == NULL ||
      config->remote_host[0] == '\0' || config->remote_port == 0U || config->local_port == 0U) {
    return false;
  }

  const network_socket_ops_t *ops = network_socket_active_ops();
  if (ops == NULL || ops->open == NULL || ops->is_ready == NULL || !ops->is_ready()) {
    LOG_WARNING("network socket open skipped: active link %s not ready", network_socket_active_link_name());
    return false;
  }

  network_socket_close(config->socket_index);
  if (!ops->open(config)) {
    return false;
  }

  socket_owners[config->socket_index].ops = ops;
  socket_owners[config->socket_index].opened = true;
  return true;
}

void network_socket_close(uint8_t socket_index) {
  if (!network_socket_index_valid(socket_index)) {
    return;
  }

  const network_socket_ops_t *ops = socket_owners[socket_index].ops;
  if (socket_owners[socket_index].opened && ops != NULL && ops->close != NULL) {
    ops->close(socket_index);
  }
  socket_owners[socket_index].ops = NULL;
  socket_owners[socket_index].opened = false;
}

void network_socket_close_all(void) {
  for (uint8_t index = 0U; index < NETWORK_SOCKET_MAX_COUNT; index++) {
    network_socket_close(index);
  }
}

bool network_socket_send(uint8_t socket_index, const uint8_t *data, uint16_t length) {
  const network_socket_ops_t *ops = network_socket_owner_ops(socket_index);
  if (ops == NULL || ops->send == NULL || data == NULL || length == 0U) {
    return false;
  }

  if (!ops->send(socket_index, data, length)) {
    socket_owners[socket_index].opened = false;
    return false;
  }
  return true;
}

bool network_socket_send_to(uint8_t socket_index, const char *host, uint16_t port, const uint8_t *data, uint16_t length) {
  const network_socket_ops_t *ops = network_socket_owner_ops(socket_index);
  if (ops == NULL || ops->send_to == NULL || host == NULL || port == 0U || data == NULL || length == 0U) {
    return false;
  }

  if (!ops->send_to(socket_index, host, port, data, length)) {
    socket_owners[socket_index].opened = false;
    return false;
  }
  return true;
}

uint16_t network_socket_recv(uint8_t socket_index, uint8_t *data, uint16_t max_length) {
  const network_socket_ops_t *ops = network_socket_owner_ops(socket_index);
  if (ops == NULL || ops->recv == NULL || data == NULL || max_length == 0U) {
    return 0U;
  }
  return ops->recv(socket_index, data, max_length);
}

bool network_socket_is_tcp_connected(uint8_t socket_index) {
  const network_socket_ops_t *ops = network_socket_owner_ops(socket_index);
  if (ops == NULL || ops->is_tcp_connected == NULL) {
    return false;
  }
  return ops->is_tcp_connected(socket_index);
}

bool network_socket_active_link_ready(void) {
  const network_socket_ops_t *ops = network_socket_active_ops();
  return ops != NULL && ops->is_ready != NULL && ops->is_ready();
}

const char *network_socket_active_link_name(void) {
  const network_socket_ops_t *ops = network_socket_active_ops();
  return ops == NULL || ops->name == NULL ? "unknown" : ops->name;
}

bool network_socket_probe_tcp_port(const char *host, uint16_t port) {
  network_socket_config_t config = {
    .socket_index = 2U,
    .proto = NETWORK_SOCKET_PROTO_TCP,
    .remote_host = host,
    .remote_port = port,
    .local_port = 49002U,
  };

  bool ok = network_socket_open(&config) && network_socket_is_tcp_connected(config.socket_index);
  network_socket_close(config.socket_index);
  return ok;
}

static const network_socket_ops_t *network_socket_ops_for_link(network_link_t link) {
  switch (link) {
  case NETWORK_LINK_CH395Q:
    return &network_socket_ch395_ops;
  case NETWORK_LINK_AIR724UG:
    return &network_socket_air724_ops;
  default:
    return NULL;
  }
}

static const network_socket_ops_t *network_socket_active_ops(void) {
  return network_socket_ops_for_link(network_manager_get_active_link());
}

static const network_socket_ops_t *network_socket_owner_ops(uint8_t socket_index) {
  if (!network_socket_index_valid(socket_index) || !socket_owners[socket_index].opened ||
      socket_owners[socket_index].ops == NULL) {
    return NULL;
  }

  const network_socket_ops_t *ops = socket_owners[socket_index].ops;
  if (ops->link != network_manager_get_active_link() || ops->is_ready == NULL || !ops->is_ready()) {
    socket_owners[socket_index].opened = false;
    return NULL;
  }
  return ops;
}

static bool network_socket_index_valid(uint8_t socket_index) {
  return socket_index < NETWORK_SOCKET_MAX_COUNT;
}
