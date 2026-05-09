#ifndef SMART_SWITCH_H
#define SMART_SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_SWITCH_MAX_CHANNELS 8U

typedef struct {
  uint16_t manufacture_model;
  uint8_t slave_addr;
  bool online;
  uint16_t input_bits;
  uint16_t output_bits;
  uint8_t last_error;
} smart_switch_t;

void smart_switch_init(smart_switch_t *device, uint16_t manufacture_model, uint8_t slave_addr);
// 智能开关实际为 M88 类工控模块：输入点和继电器输出分别读 0x0000/0x0001。
bool smart_switch_poll(smart_switch_t *device);
// relay_bits 低 8 位对应 1..8 路继电器，保持旧工程全开 0x00FF、全关 0x0000 语义。
bool smart_switch_set_all(smart_switch_t *device, uint16_t relay_bits);
bool smart_switch_set_one(smart_switch_t *device, uint8_t channel, bool enabled);
uint8_t smart_switch_get_input(const smart_switch_t *device, uint8_t channel);
uint8_t smart_switch_get_output(const smart_switch_t *device, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif
