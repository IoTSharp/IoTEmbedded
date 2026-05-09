#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include "Common/Inc/app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_SERVER_IP_BUF_SIZE 64
#define MQTT_CLIENT_ID_BUF_SIZE 26
#define MQTT_USER_NAME_BUF_SIZE 25
#define MQTT_PASSWORD_BUF_SIZE  33
#define NTP_SERVER_IP_BUF_SIZE  64
#define NETWORK_PROBE_HOST_BUF_SIZE 64

/** 网络工作模式：auto 保留主备自动切换，wired/4G 用于现场强制固定链路并写入 EEPROM。 */
typedef enum {
  NETWORK_MODE_AUTO = 0,
  NETWORK_MODE_CH395Q,
  NETWORK_MODE_AIR724UG,
} network_mode_t;

typedef struct {
  char ip[MQTT_SERVER_IP_BUF_SIZE];
  u16 port;
  char client_id[MQTT_CLIENT_ID_BUF_SIZE];
  char user_name[MQTT_USER_NAME_BUF_SIZE];
  char password[MQTT_PASSWORD_BUF_SIZE];
  u16 keepalive;
  u8 sub_qos;
  u8 pub_qos;
  char local_ip[MQTT_SERVER_IP_BUF_SIZE];
  char gateway_ip[MQTT_SERVER_IP_BUF_SIZE];
  char mask_ip[MQTT_SERVER_IP_BUF_SIZE];
  u16 local_port;
} mqtt_config_t;

typedef struct {
  char ip[NTP_SERVER_IP_BUF_SIZE];
  u16 port;
  u16 local_port;
  int time_zone;
} ntp_config_t;

typedef struct {
  u32 main_loop_interval;
  u32 keep_in_touch_with_server_interval;
  u32 sync_network_time_interval;
  u32 req_devices_reg_info_interval;
  u32 process_downlink_data_interval;
  u32 report_devices_data_interval;
  u32 report_alarm_data_interval;
  u32 process_uart_cmd_interval;
  u16 feed_watch_dog_interval;
  u16 feed_watch_dog_max_retries;
} loop_config_t;

typedef struct {
  char probe_host[NETWORK_PROBE_HOST_BUF_SIZE];
  u16 probe_port;
  u32 probe_interval_ms;
} network_monitor_config_t;

#ifdef __cplusplus
}
#endif

#endif
