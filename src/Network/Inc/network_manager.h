#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Config/Inc/network_config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NETWORK_LINK_NONE = 0,
  NETWORK_LINK_CH395Q,
  NETWORK_LINK_AIR724UG,
  NETWORK_LINK_AP6181,
} network_link_t;

typedef enum {
  NETWORK_STATE_UNKNOWN = 0,
  NETWORK_STATE_UNAVAILABLE,
  NETWORK_STATE_CH395Q_ACTIVE,
  NETWORK_STATE_AIR724UG_ACTIVE,
  NETWORK_STATE_AP6181_ACTIVE,
} network_state_t;

void network_manager_init(const network_monitor_config_t *config, network_mode_t mode);
void network_manager_poll(void);
network_state_t network_manager_get_state(void);
network_link_t network_manager_get_active_link(void);
network_mode_t network_manager_get_mode(void);
void network_manager_set_mode(network_mode_t mode);
bool network_manager_ch395q_probe_now(void);
void network_manager_force_probe(void);
void network_manager_force_use_ch395q(void);
void network_manager_force_use_air724ug(void);

/* 切换/探测前给上层一次准备 CH395Q 的机会。
 * 默认实现只释放 RSTI；app.c 会用当前 IP/网关/掩码重做 CH395Q 初始化。 */
bool network_prepare_ch395q_probe(void);
bool network_probe_ch395q_port(const char *host, uint16_t port);
void network_switch_to_ch395q(void);
void network_switch_to_air724ug(void);
void network_switch_to_ap6181(void);

#ifdef __cplusplus
}
#endif

#endif
