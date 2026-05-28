#include "Network/Inc/network_manager.h"

#include "Modem/Inc/bsp_air724.h"
#include "Board/Inc/bsp_board.h"
#include "Network/Ap6181/Inc/bsp_ap6181.h"
#include "Network/Ch395/Inc/bsp_ch395.h"
#include "Network/Ch395/Inc/ch395_driver.h"
#include "Common/Inc/log.h"
#include "Network/Inc/network_socket.h"
#include <string.h>

#define NETWORK_DEFAULT_PROBE_INTERVAL_MS (5UL * 60UL * 1000UL)

static network_monitor_config_t monitor_config;
static network_state_t current_state = NETWORK_STATE_UNKNOWN;
static network_link_t active_link = NETWORK_LINK_NONE;
static network_mode_t active_mode = NETWORK_MODE_AUTO;
static uint32_t last_probe_tick;
static bool force_probe_requested;

static network_link_t network_manager_default_link(void);
static void network_manager_select_ch395q(void);
static void network_manager_select_air724ug(void);
static void network_manager_select_ap6181(void);
static network_mode_t network_manager_normalize_mode(network_mode_t mode);
static uint32_t network_manager_probe_interval(void);

void network_manager_init(const network_monitor_config_t *config, network_mode_t mode) {
  memset(&monitor_config, 0, sizeof(monitor_config));
  if (config != NULL) {
    monitor_config = *config;
  }

  if (monitor_config.probe_interval_ms == 0U) {
    monitor_config.probe_interval_ms = NETWORK_DEFAULT_PROBE_INTERVAL_MS;
  }

  current_state = NETWORK_STATE_UNKNOWN;
  active_link = network_manager_default_link();
  active_mode = network_manager_normalize_mode(mode);
  last_probe_tick = 0U;
  force_probe_requested = active_mode == NETWORK_MODE_AUTO;
  /* auto 保留原有主备探测；固定模式直接做复位隔离，避免重启后先误跑另一条链路。 */
  if (active_mode == NETWORK_MODE_CH395Q) {
    network_manager_select_ch395q();
  } else if (active_mode == NETWORK_MODE_AIR724UG) {
    network_manager_select_air724ug();
  } else if (active_mode == NETWORK_MODE_WIFI) {
    network_manager_select_ap6181();
  }
}

void network_manager_poll(void) {
  if (active_mode == NETWORK_MODE_CH395Q) {
    if (force_probe_requested || active_link != NETWORK_LINK_CH395Q || current_state != NETWORK_STATE_CH395Q_ACTIVE) {
      force_probe_requested = false;
      network_manager_select_ch395q();
    }
    return;
  }

  if (active_mode == NETWORK_MODE_AIR724UG) {
    if (force_probe_requested || active_link != NETWORK_LINK_AIR724UG ||
        current_state != NETWORK_STATE_AIR724UG_ACTIVE) {
      force_probe_requested = false;
      network_manager_select_air724ug();
    }
    return;
  }

  if (active_mode == NETWORK_MODE_WIFI) {
    if (force_probe_requested || active_link != NETWORK_LINK_AP6181 || current_state != NETWORK_STATE_AP6181_ACTIVE) {
      force_probe_requested = false;
      network_manager_select_ap6181();
    }
    return;
  }

#if BSP_HAS_AP6181
  if (active_link != NETWORK_LINK_AP6181 || current_state != NETWORK_STATE_AP6181_ACTIVE) {
    network_manager_select_ap6181();
  }
  return;
#endif

  uint32_t now = bsp_get_tick_ms();
  uint32_t interval = network_manager_probe_interval();

  if (!force_probe_requested && (now - last_probe_tick) < interval) {
    return;
  }

  force_probe_requested = false;
  last_probe_tick = now;

  if (network_manager_ch395q_probe_now()) {
    if (active_link != NETWORK_LINK_CH395Q || current_state != NETWORK_STATE_CH395Q_ACTIVE) {
      network_manager_select_ch395q();
    }
    return;
  }

  if (active_link != NETWORK_LINK_AIR724UG || current_state != NETWORK_STATE_AIR724UG_ACTIVE) {
    network_manager_select_air724ug();
  }
}

network_state_t network_manager_get_state(void) {
  return current_state;
}

network_link_t network_manager_get_active_link(void) {
  return active_link;
}

network_mode_t network_manager_get_mode(void) {
  return active_mode;
}

void network_manager_set_mode(network_mode_t mode) {
  active_mode = network_manager_normalize_mode(mode);
  if (active_mode == NETWORK_MODE_CH395Q) {
    network_manager_select_ch395q();
  } else if (active_mode == NETWORK_MODE_AIR724UG) {
    network_manager_select_air724ug();
  } else if (active_mode == NETWORK_MODE_WIFI) {
    network_manager_select_ap6181();
  } else {
    force_probe_requested = true;
  }
}

bool network_manager_ch395q_probe_now(void) {
  network_link_t link_before_probe = active_link;

  if (!network_prepare_ch395q_probe()) {
    if (link_before_probe == NETWORK_LINK_AIR724UG) {
      network_switch_to_air724ug();
    }
    return false;
  }

  bool ok = network_probe_ch395q_port(monitor_config.probe_host, monitor_config.probe_port);
  if (link_before_probe == NETWORK_LINK_AIR724UG) {
    /* 只是探测时，要把“探测前的隔离态”恢复回去，避免 Air724UG 留在半开状态。 */
    network_switch_to_air724ug();
  }
  return ok;
}

void network_manager_force_probe(void) {
  force_probe_requested = true;
}

void network_manager_force_use_ch395q(void) {
  network_manager_set_mode(NETWORK_MODE_CH395Q);
}

void network_manager_force_use_air724ug(void) {
  network_manager_set_mode(NETWORK_MODE_AIR724UG);
}

static void network_manager_select_ch395q(void) {
  network_socket_close_all();
#if !BSP_HAS_CH395Q
  active_link = NETWORK_LINK_NONE;
  current_state = NETWORK_STATE_UNAVAILABLE;
  LOG_WARNING("CH395Q unavailable on this board profile");
  return;
#endif
  if (!network_prepare_ch395q_probe()) {
    LOG_WARNING("CH395Q prepare failed before switch, forcing CH395Q isolation");
  }
  network_switch_to_ch395q();
  active_link = NETWORK_LINK_CH395Q;
  current_state = NETWORK_STATE_CH395Q_ACTIVE;
  LOG_INFO("network switched to CH395Q/UART4/CN2; Air724UG held in reset");
}

static void network_manager_select_air724ug(void) {
  network_socket_close_all();
#if !BSP_HAS_AIR724UG
  active_link = NETWORK_LINK_NONE;
  current_state = NETWORK_STATE_UNAVAILABLE;
  LOG_WARNING("Air724UG unavailable on this board profile");
  return;
#endif
  network_switch_to_air724ug();
  bsp_delay_ms(1000U);
  active_link = NETWORK_LINK_AIR724UG;
  current_state = NETWORK_STATE_AIR724UG_ACTIVE;
  if (active_mode == NETWORK_MODE_AIR724UG) {
    LOG_INFO("network switched to Air724UG/UART4 by fixed mode; CH395Q held in reset");
  } else {
    LOG_WARNING("CH395Q probe failed, network switched to Air724UG/UART4; CH395Q held in reset");
  }
}

static void network_manager_select_ap6181(void) {
  network_socket_close_all();
#if !BSP_HAS_AP6181
  active_link = NETWORK_LINK_NONE;
  current_state = NETWORK_STATE_UNAVAILABLE;
  LOG_WARNING("AP6181 WiFi unavailable on this board profile");
  return;
#endif
  network_switch_to_ap6181();
  active_link = NETWORK_LINK_AP6181;
  current_state = NETWORK_STATE_AP6181_ACTIVE;
  LOG_INFO("network switched to AP6181 WiFi/SDMMC1; wired/4G fallback disabled");
}

static network_mode_t network_manager_normalize_mode(network_mode_t mode) {
  switch (mode) {
  case NETWORK_MODE_CH395Q:
#if BSP_HAS_CH395Q
    return mode;
#else
    return NETWORK_MODE_AUTO;
#endif
  case NETWORK_MODE_AIR724UG:
#if BSP_HAS_AIR724UG
    return mode;
#else
    return NETWORK_MODE_AUTO;
#endif
  case NETWORK_MODE_WIFI:
#if BSP_HAS_AP6181
    return mode;
#else
    return NETWORK_MODE_AUTO;
#endif
  case NETWORK_MODE_AUTO:
  default:
#if BSP_HAS_AP6181
    return NETWORK_MODE_WIFI;
#else
    return NETWORK_MODE_AUTO;
#endif
  }
}

static network_link_t network_manager_default_link(void) {
#if BSP_HAS_AP6181
  return NETWORK_LINK_AP6181;
#elif BSP_HAS_CH395Q
  return NETWORK_LINK_CH395Q;
#elif BSP_HAS_AIR724UG
  return NETWORK_LINK_AIR724UG;
#else
  return NETWORK_LINK_NONE;
#endif
}

static uint32_t network_manager_probe_interval(void) {
  return monitor_config.probe_interval_ms == 0U ? NETWORK_DEFAULT_PROBE_INTERVAL_MS : monitor_config.probe_interval_ms;
}

__attribute__((weak)) bool network_probe_ch395q_port(const char *host, uint16_t port) {
  (void)host;
  (void)port;
  return false;
}

__attribute__((weak)) bool network_prepare_ch395q_probe(void) {
#if BSP_HAS_CH395Q
  bsp_ch395_release_reset();
  return true;
#else
  return false;
#endif
}

__attribute__((weak)) void network_switch_to_ch395q(void) {
#if BSP_HAS_CH395Q
  /* 当前硬件没有确认到两个模块的电源 EN，只能用复位脚做总线/链路隔离。
   * 若后续 PCB 增加 load-switch，可在这里替换为真正的电源门控。 */
  bsp_air724_assert_reset();
  bsp_ch395_release_reset();
#endif
}

__attribute__((weak)) void network_switch_to_air724ug(void) {
#if BSP_HAS_AIR724UG
  /* UART4 被 CH395Q/CN2 和 Air724UG 复用时，只允许当前活动模块脱离复位，
   * 避免两个模块同时驱动同一组 TX/RX 线。 */
  bsp_ch395_assert_reset();
  bsp_air724_release_reset();
  (void)bsp_air724_read_netstate();
#endif
}

__attribute__((weak)) void network_switch_to_ap6181(void) {
#if BSP_HAS_AP6181
  bsp_ap6181_prepare_pins();
  bsp_ap6181_enable();
#endif
}
