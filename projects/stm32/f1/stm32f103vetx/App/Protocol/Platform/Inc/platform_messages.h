#ifndef PLATFORM_MESSAGES_H
#define PLATFORM_MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_MESSAGE_MAX_LEN 384U

void platform_messages_init(void);
void platform_messages_handle_downlink(const char *topic, const char *payload);
bool platform_messages_request_register_info(void);
bool platform_messages_request_register_info_as_needed(uint32_t now_ms, uint32_t interval_ms);
bool platform_messages_publish_heartbeat(uint32_t current_time);
bool platform_messages_publish_command_response(const char *command_id, int result, const char *message);
void platform_messages_set_registered(bool registered);
bool platform_messages_is_registered(void);
const char *platform_messages_last_command(void);
const char *platform_messages_last_register_info(void);
const char *platform_messages_last_update_response(void);

#ifdef __cplusplus
}
#endif

#endif
