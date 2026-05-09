#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "network_config.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_TOPIC_MAX_LEN 128U
// 多个 RS485 子设备同批上报时，512 字节只够早期温湿度样板；现场默认表需要更大的 /datas/ JSON 空间。
#define MQTT_PAYLOAD_MAX_LEN 1536U

typedef enum {
  MQTT_CLIENT_STATE_DISCONNECTED = 0,
  MQTT_CLIENT_STATE_TCP_CONNECTED,
  MQTT_CLIENT_STATE_SESSION_CONNECTED,
  MQTT_CLIENT_STATE_SUBSCRIBED,
} mqtt_client_state_t;

typedef enum {
  MQTT_PACKET_FLAG_CONNACK = (1U << 0),
  MQTT_PACKET_FLAG_PUBLISH = (1U << 1),
  MQTT_PACKET_FLAG_PUBACK = (1U << 2),
  MQTT_PACKET_FLAG_SUBACK = (1U << 3),
  MQTT_PACKET_FLAG_PINGRESP = (1U << 4),
  MQTT_PACKET_FLAG_DISCONNECT = (1U << 5),
} mqtt_packet_flags_t;

typedef void (*mqtt_message_handler_t)(const char *topic, const char *payload);

void mqtt_client_init(mqtt_config_t *config);
bool mqtt_client_connect(void);
void mqtt_client_disconnect(void);
bool mqtt_client_subscribe_platform_topics(void);
bool mqtt_client_publish_get_device_info(const char *payload);
bool mqtt_client_publish_update(const char *payload);
bool mqtt_client_publish_data(const char *payload);
bool mqtt_client_publish_command_response(const char *payload);
bool mqtt_client_ping(void);
void mqtt_client_poll(void);
// 主循环维护入口：负责接收下行、断线重连、自动订阅和 keepalive PING。
bool mqtt_client_maintain(uint32_t now_ms, uint32_t reconnect_interval_ms);
void mqtt_client_set_message_handler(mqtt_message_handler_t handler);
mqtt_client_state_t mqtt_client_get_state(void);
uint16_t mqtt_client_get_packet_flags(void);
void mqtt_client_clear_packet_flags(uint16_t flags);
uint16_t mqtt_client_get_last_puback_id(void);
bool mqtt_client_is_ready(void);
void mqtt_client_build_topic(char *buffer, uint16_t buffer_len, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif
