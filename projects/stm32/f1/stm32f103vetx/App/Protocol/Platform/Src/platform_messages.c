#include "platform_messages.h"

#include "config.h"
#include "device_manager.h"
#include "log.h"
#include "mqtt_client.h"
#include "parson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_TO_CLIENT_REGISTER_INFO "registerInfo"
#define SERVER_TO_CLIENT_UPDATE_RESP "updateResponse"
#define SERVER_TO_CLIENT_CMD "command"

static bool devices_registered;
static uint32_t last_register_request_tick;
static char last_command[PLATFORM_MESSAGE_MAX_LEN];
static char last_register_info[PLATFORM_MESSAGE_MAX_LEN];
// 保留平台 updateResponse 原文，便于现场确认平台是否已经回了更新确认。
static char last_update_response[PLATFORM_MESSAGE_MAX_LEN];

static bool platform_topic_contains(const char *topic, const char *token);
static void platform_copy_payload(char *dst, uint16_t dst_len, const char *payload);
static bool platform_serialize_to_buffer(JSON_Value *root_value, char *buffer, uint16_t buffer_len);
// 平台下行 JSON 先统一做对象校验，再交给具体分支解析，避免异常 payload 直接打到空指针。
static JSON_Value *platform_parse_object_payload(const char *payload, JSON_Object **root);
static bool platform_messages_apply_register_info(const char *payload);
static void platform_messages_execute_command(const char *payload);

void platform_messages_init(void) {
  devices_registered = false;
  last_register_request_tick = 0U;
  memset(last_command, 0, sizeof(last_command));
  memset(last_register_info, 0, sizeof(last_register_info));
  memset(last_update_response, 0, sizeof(last_update_response));
  mqtt_client_set_message_handler(platform_messages_handle_downlink);
}

void platform_messages_handle_downlink(const char *topic, const char *payload) {
  if (topic == NULL || payload == NULL) {
    return;
  }
  if (platform_topic_contains(topic, SERVER_TO_CLIENT_REGISTER_INFO)) {
    platform_copy_payload(last_register_info, sizeof(last_register_info), payload);
    devices_registered = platform_messages_apply_register_info(payload);
    LOG_INFO("platform registerInfo received, apply=%u", devices_registered ? 1U : 0U);
  } else if (platform_topic_contains(topic, SERVER_TO_CLIENT_UPDATE_RESP)) {
    platform_copy_payload(last_update_response, sizeof(last_update_response), payload);
    devices_registered = true;
    LOG_INFO("platform updateResponse received");
  } else if (platform_topic_contains(topic, SERVER_TO_CLIENT_CMD)) {
    platform_copy_payload(last_command, sizeof(last_command), payload);
    LOG_INFO("platform command received");
    platform_messages_execute_command(payload);
  } else {
    LOG_DEBUG("platform downlink ignored: %s", topic);
  }
}

bool platform_messages_request_register_info(void) {
  char message[PLATFORM_MESSAGE_MAX_LEN] = {0};
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(root_value);
  json_object_set_string(root, "type", "getDeviceInfo");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  if (platform_serialize_to_buffer(root_value, message, sizeof(message)) && mqtt_client_publish_get_device_info(message)) {
    last_register_request_tick = HAL_GetTick();
    json_value_free(root_value);
    return true;
  }
  json_value_free(root_value);
  return false;
}

bool platform_messages_request_register_info_as_needed(uint32_t now_ms, uint32_t interval_ms) {
  if (devices_registered) {
    return true;
  }
  if (last_register_request_tick == 0U || (now_ms - last_register_request_tick) >= interval_ms) {
    return platform_messages_request_register_info();
  }
  return false;
}

bool platform_messages_publish_heartbeat(uint32_t current_time) {
  char message[PLATFORM_MESSAGE_MAX_LEN] = {0};
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(root_value);
  json_object_set_string(root, "type", "heartbeat");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  json_object_set_number(root, "time", current_time);
  json_object_set_boolean(root, "registered", devices_registered ? 1 : 0);
  bool ok = platform_serialize_to_buffer(root_value, message, sizeof(message)) && mqtt_client_publish_update(message);
  json_value_free(root_value);
  return ok;
}

bool platform_messages_publish_command_response(const char *command_id, int result, const char *message) {
  char payload[PLATFORM_MESSAGE_MAX_LEN] = {0};
  uint16_t mid = 0U;
  char *end_ptr = NULL;
  if (command_id != NULL && command_id[0] != '\0') {
    unsigned long parsed = strtoul(command_id, &end_ptr, 10);
    if (end_ptr != command_id && parsed <= 0xFFFFUL) {
      mid = (uint16_t)parsed;
    }
  }
  // 新字段 commandId 便于后续平台演进；mid/statusCode 保留旧 excv_cmd.echo_cmd 响应语义。
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root = json_value_get_object(root_value);
  json_object_set_string(root, "type", "commandResponse");
  json_object_set_string(root, "collectorId", active_config.mqtt.user_name);
  json_object_set_string(root, "commandId", command_id == NULL ? "" : command_id);
  json_object_set_number(root, "mid", mid);
  json_object_set_number(root, "statusCode", result == 0 ? 0 : 1);
  json_object_set_number(root, "result", result);
  json_object_set_string(root, "message", message == NULL ? "" : message);
  bool ok = platform_serialize_to_buffer(root_value, payload, sizeof(payload)) && mqtt_client_publish_command_response(payload);
  json_value_free(root_value);
  return ok;
}

void platform_messages_set_registered(bool registered) {
  devices_registered = registered;
}

bool platform_messages_is_registered(void) {
  return devices_registered;
}

const char *platform_messages_last_command(void) {
  return last_command;
}

const char *platform_messages_last_register_info(void) {
  return last_register_info;
}

const char *platform_messages_last_update_response(void) {
  return last_update_response;
}

static bool platform_topic_contains(const char *topic, const char *token) {
  return topic != NULL && token != NULL && strstr(topic, token) != NULL;
}

static void platform_copy_payload(char *dst, uint16_t dst_len, const char *payload) {
  if (dst == NULL || dst_len == 0U) {
    return;
  }
  (void)snprintf(dst, dst_len, "%s", payload == NULL ? "" : payload);
}

static bool platform_serialize_to_buffer(JSON_Value *root_value, char *buffer, uint16_t buffer_len) {
  if (root_value == NULL || buffer == NULL || buffer_len == 0U) {
    return false;
  }
  return json_serialize_to_buffer(root_value, buffer, buffer_len) == JSONSuccess;
}

static JSON_Value *platform_parse_object_payload(const char *payload, JSON_Object **root) {
  JSON_Value *root_value = json_parse_string(payload == NULL ? "" : payload);
  if (root != NULL) {
    *root = NULL;
  }
  if (root_value == NULL) {
    return NULL;
  }
  JSON_Object *object = json_value_get_object(root_value);
  if (object == NULL) {
    json_value_free(root_value);
    return NULL;
  }
  if (root != NULL) {
    *root = object;
  }
  return root_value;
}

static bool platform_messages_apply_register_info(const char *payload) {
  uint16_t configured_count = 0U;
  JSON_Object *root = NULL;
  JSON_Value *root_value = platform_parse_object_payload(payload, &root);
  if (root_value == NULL || root == NULL) {
    json_value_free(root_value);
    return false;
  }
  JSON_Array *device_infos = json_object_get_array(root, "deviceInfos");
  if (device_infos == NULL) {
    json_value_free(root_value);
    return false;
  }

  device_manager_clear_registered_devices();
  for (size_t i = 0U; i < json_array_get_count(device_infos); i++) {
    JSON_Object *item = json_array_get_object(device_infos, i);
    uint16_t serial_port = (uint16_t)json_object_get_number(item, "serialPort");
    uint16_t service_id = (uint16_t)json_object_get_number(item, "serviceId");
    uint16_t brand = (uint16_t)json_object_get_number(item, "brand");
    if (item != NULL && serial_port != 0U && service_id != 0U &&
        !config_saved_device_table_allows(service_id, brand, (uint8_t)serial_port)) {
      LOG_WARNING("device register item skipped by saved config, serviceId=%u brand=%u addr=%u", service_id, brand,
                  serial_port);
      continue;
    }
    if (item != NULL && serial_port != 0U && service_id != 0U &&
        device_manager_configure_registered_device(service_id, brand, (uint8_t)serial_port)) {
      configured_count++;
      LOG_INFO("device registered: serviceId=%u brand=%u addr=%u", service_id, brand, serial_port);
    } else {
      LOG_ERROR("device register item unsupported or invalid, index=%u", (unsigned int)i);
    }
  }
  json_value_free(root_value);
  return configured_count > 0U;
}

static void platform_messages_execute_command(const char *payload) {
  JSON_Object *root = NULL;
  JSON_Value *root_value = platform_parse_object_payload(payload, &root);
  char command_id[24] = {0};
  if (root_value == NULL || root == NULL) {
    (void)platform_messages_publish_command_response("", -1, "invalid json");
    json_value_free(root_value);
    return;
  }

  // 兼容旧平台字段：mid 用于旧响应关联，commandId 用于新响应关联，二者都可能出现。
  uint16_t mid = (uint16_t)json_object_get_number(root, "mid");
  uint16_t serial_port = (uint16_t)json_object_get_number(root, "serialPort");
  uint16_t service_id = (uint16_t)json_object_get_number(root, "serviceId");
  uint16_t command = (uint16_t)json_object_get_number(root, "cmd");
  uint16_t param = (uint16_t)json_object_get_number(root, "params");
  const char *json_command_id = json_object_get_string(root, "commandId");
  if (json_command_id != NULL) {
    (void)snprintf(command_id, sizeof(command_id), "%s", json_command_id);
  } else {
    (void)snprintf(command_id, sizeof(command_id), "%u", mid);
  }

  if (service_id == 0U || command == 0U) {
    (void)platform_messages_publish_command_response(command_id, -1, "invalid command");
    json_value_free(root_value);
    return;
  }

  // 平台命令统一交给设备层分发；当前已覆盖智能开关和空调控制器，未知 serviceId/cmd 会返回 failed。
  bool ok = device_manager_execute_command(service_id, (uint8_t)serial_port, command, param);
  (void)platform_messages_publish_command_response(command_id, ok ? 0 : -1, ok ? "executed" : "failed");
  if (ok) {
    (void)device_manager_poll_all();
    if (mqtt_client_is_ready()) {
      (void)device_manager_publish_all();
    }
  }
  json_value_free(root_value);
}
