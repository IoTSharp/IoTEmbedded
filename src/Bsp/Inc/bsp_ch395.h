#ifndef BSP_CH395_H
#define BSP_CH395_H

#include "bsp_board.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 控制 CH395Q 外部复位输入 RSTI/RST1；PA5/RST 是芯片输出脚，不能由 MCU 驱动。
void bsp_ch395_assert_reset(void);
void bsp_ch395_release_reset(void);
bool bsp_ch395_is_reset_asserted(void);

// 按最新原理图准备 CH395Q 启动相关引脚：SCS 空闲高、SEL 高、RST 输出脚保持输入。
void bsp_ch395_prepare_boot_pins(void);
void bsp_ch395_reset(void);

#ifdef __cplusplus
}
#endif

#endif
