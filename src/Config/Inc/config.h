#ifndef CONFIG_H
#define CONFIG_H

#include "Common/Inc/app_types.h"
#include "Common/Inc/log.h"
#include "Common/Inc/md5.h"
#include "Config/Inc/network_config.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_VERSION "v0.0.5-device-table"
#define MAGIC_NUMBER_SIZE 4
#define CONFIG_CMD_RECV_BUF_SIZE 256
#define CONFIG_AUTH_PASSWORD_BUF_SIZE 32
#define CONFIG_DEVICE_TABLE_MAX 9U

/** 串口 Shell 本地账号配置：当前只在调试串口使用，root 可维护配置，user 只读。 */
typedef struct {
  char root_password[CONFIG_AUTH_PASSWORD_BUF_SIZE];
  char user_password[CONFIG_AUTH_PASSWORD_BUF_SIZE];
} config_auth_t;

/** 现场 RS485 子设备配置项：serviceId/型号/从站地址来自平台 registerInfo 或上位机手工勾选。 */
typedef struct {
  // 平台 serviceId，必须与 device_catalog.h 中的枚举一致。
  u16 service_id;
  // 厂商/型号字段，对 UPS、空调等多协议设备尤其关键；无型号时为 0。
  u16 manufacture_model;
  // Modbus 从站地址，0 表示该槽位未使用。
  u8 slave_addr;
  // 该槽位是否参与启动恢复；保留显式开关，便于后续支持“保留但禁用”。
  u8 enabled;
} config_device_entry_t;

/** EEPROM 中保存的现场设备表；默认空表，必须由上位机或平台信息明确注册。 */
typedef struct {
  u8 count;
  u8 reserved[3];
  config_device_entry_t entries[CONFIG_DEVICE_TABLE_MAX];
} config_device_table_t;

typedef struct {
  char magic_number[MAGIC_NUMBER_SIZE];
  config_auth_t auth;
  mqtt_config_t mqtt;
  ntp_config_t ntp;
  loop_config_t loop;
  network_monitor_config_t network_monitor;
  network_mode_t network_mode;
  config_device_table_t devices;
  log_config_t log;
  char md5[MD5_STR_LEN + 1];
  u16 crc;
} config_t;

extern config_t active_config;
extern char config_cmd_recv_buf[CONFIG_CMD_RECV_BUF_SIZE];
extern __IO u16 config_cmd_recv_len;

void config_init(void);
ErrorStatus config_write_into_eeprom(void);
void config_process_cmd(void);
void config_receive_cmd_byte(uint8_t byte);
void config_reset_to_default(void);
ErrorStatus config_get_value(const char *key, char *buf, size_t buf_size);
ErrorStatus config_set_value(const char *key, const char *value);
ErrorStatus config_apply_runtime(void);
ErrorStatus config_apply_network_mode(network_mode_t mode, bool persist);
ErrorStatus config_apply_ch395q_network(bool persist);
network_mode_t config_get_network_mode(void);
const char *config_network_mode_name(network_mode_t mode);
bool config_parse_network_mode(const char *text, network_mode_t *mode);
/** 平台 registerInfo 进入运行态前先检查手工保存的现场设备白名单；空表时允许平台注册用于首次联调。 */
bool config_saved_device_table_allows(uint16_t service_id, uint16_t manufacture_model, uint8_t slave_addr);

#ifdef __cplusplus
}
#endif

#endif
