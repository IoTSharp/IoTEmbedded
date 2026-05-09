#include "network_time.h"

#include "bsp_board.h"
#include "log.h"
#include "network_socket.h"

#include <stddef.h>
#include <string.h>

#define NTP_PACKET_SIZE 48U
#define NTP_SOCKET_INDEX 1U
#define NTP_UNIX_EPOCH_DELTA 2208988800UL
#define NTP_RECV_WAIT_MS 1500U

static network_time_status_t time_status = {0};

static bool network_time_build_config(const ntp_config_t *config, network_socket_config_t *socket_config);
static bool network_time_wait_response(uint8_t socket_index, uint8_t packet[NTP_PACKET_SIZE]);
static void network_time_set_rtc_seconds(uint32_t unix_time);

bool network_time_sync_ntp(const ntp_config_t *config) {
  network_socket_config_t socket_config = {0};
  uint8_t packet[NTP_PACKET_SIZE] = {0};

  if (!network_time_build_config(config, &socket_config)) {
    return false;
  }

  if (!network_socket_active_link_ready()) {
    LOG_WARNING("NTP skipped: active network %s not ready", network_socket_active_link_name());
    return false;
  }

  if (!network_socket_open(&socket_config)) {
    LOG_WARNING("NTP socket open failed");
    return false;
  }

  packet[0] = 0x1BU;
  bool ok = network_socket_send(socket_config.socket_index, packet, NTP_PACKET_SIZE);
  if (ok) {
    memset(packet, 0, sizeof(packet));
    ok = network_time_wait_response(socket_config.socket_index, packet);
  }

  network_socket_close(socket_config.socket_index);

  if (!ok) {
    LOG_WARNING("NTP sync failed");
    return false;
  }

  uint32_t seconds = ((uint32_t)packet[40] << 24) | ((uint32_t)packet[41] << 16) | ((uint32_t)packet[42] << 8) |
                     (uint32_t)packet[43];
  if (seconds <= NTP_UNIX_EPOCH_DELTA) {
    LOG_WARNING("NTP response invalid");
    return false;
  }

  uint32_t unix_time = seconds - NTP_UNIX_EPOCH_DELTA;
  if (config != NULL) {
    unix_time += (uint32_t)(config->time_zone * 900);
  }

  network_time_set_rtc_seconds(unix_time);
  time_status.valid = true;
  time_status.unix_time = unix_time;
  time_status.hour = (uint8_t)((unix_time / 3600UL) % 24UL);
  time_status.minute = (uint8_t)((unix_time / 60UL) % 60UL);
  time_status.second = (uint8_t)(unix_time % 60UL);
  LOG_INFO("NTP sync ok: %02u:%02u:%02u", time_status.hour, time_status.minute, time_status.second);
  return true;
}

network_time_status_t network_time_get_status(void) {
  return time_status;
}

static bool network_time_build_config(const ntp_config_t *config, network_socket_config_t *socket_config) {
  if (config == NULL || socket_config == NULL || config->port == 0U || config->local_port == 0U) {
    return false;
  }

  memset(socket_config, 0, sizeof(*socket_config));
  socket_config->socket_index = NTP_SOCKET_INDEX;
  socket_config->proto = NETWORK_SOCKET_PROTO_UDP;
  socket_config->remote_host = config->ip;
  socket_config->remote_port = config->port;
  socket_config->local_port = config->local_port;
  return config->ip[0] != '\0';
}

static bool network_time_wait_response(uint8_t socket_index, uint8_t packet[NTP_PACKET_SIZE]) {
  uint32_t start = bsp_get_tick_ms();
  do {
    uint16_t length = network_socket_recv(socket_index, packet, NTP_PACKET_SIZE);
    if (length >= NTP_PACKET_SIZE) {
      return true;
    }
    bsp_delay_ms(20U);
  } while ((bsp_get_tick_ms() - start) < NTP_RECV_WAIT_MS);
  return false;
}

static void network_time_set_rtc_seconds(uint32_t unix_time) {
  RTC_TimeTypeDef time = {0};
  time.Hours = (uint8_t)((unix_time / 3600UL) % 24UL);
  time.Minutes = (uint8_t)((unix_time / 60UL) % 60UL);
  time.Seconds = (uint8_t)(unix_time % 60UL);
  (void)HAL_RTC_SetTime(BSP_RTC_HANDLE, &time, RTC_FORMAT_BIN);
}
