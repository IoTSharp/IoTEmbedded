#ifndef CH395_BOARD_H
#define CH395_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool present;
  uint8_t check_result;
  uint8_t version;
  uint8_t phy_status;
  uint8_t init_status;
} ch395_board_status_t;

void ch395_board_init_default(void);
bool ch395_board_init_network(const char *local_ip, const char *gateway_ip, const char *mask_ip);

ch395_board_status_t ch395_board_get_status(void);
void ch395_board_log_status(void);

#ifdef __cplusplus
}
#endif

#endif
