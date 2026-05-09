#ifndef CH395_DEFS_H
#define CH395_DEFS_H

#include <stdint.h>

#define CH395_CMD_GET_IC_VER             0x01U
#define CH395_CMD_SET_BAUDRATE           0x02U
#define CH395_CMD_RESET_ALL              0x05U
#define CH395_CMD_CHECK_EXIST            0x06U
#define CH395_CMD_SET_MAC_ADDR           0x21U
#define CH395_CMD_SET_IP_ADDR            0x22U
#define CH395_CMD_SET_GWIP_ADDR          0x23U
#define CH395_CMD_SET_MASK_ADDR          0x24U
#define CH395_CMD_GET_PHY_STATUS         0x26U
#define CH395_CMD_INIT_CH395             0x27U
#define CH395_CMD_GET_CMD_STATUS         0x2CU
#define CH395_CMD_CLEAR_RECV_BUF_SN      0x2EU
#define CH395_CMD_GET_SOCKET_STATUS_SN   0x2FU
#define CH395_CMD_SET_IP_ADDR_SN         0x31U
#define CH395_CMD_SET_DES_PORT_SN        0x32U
#define CH395_CMD_SET_SOUR_PORT_SN       0x33U
#define CH395_CMD_SET_PROTO_TYPE_SN      0x34U
#define CH395_CMD_OPEN_SOCKET_SN         0x35U
#define CH395_CMD_TCP_CONNECT_SN         0x37U
#define CH395_CMD_TCP_DISCONNECT_SN      0x38U
#define CH395_CMD_WRITE_SEND_BUF_SN      0x39U
#define CH395_CMD_GET_SOCKET_INT_SN      0x30U
#define CH395_CMD_GET_RECV_LEN_SN        0x3BU
#define CH395_CMD_READ_RECV_BUF_SN       0x3CU
#define CH395_CMD_CLOSE_SOCKET_SN        0x3DU

#define CH395_CMD_ERR_SUCCESS            0x00U
#define CH395_ERR_BUSY                   0x10U
#define CH395_ERR_ISCONN                 0x1DU
#define CH395_ERR_OPEN                   0x20U
#define CH395_ERR_UNKNOW                 0xFAU

#define CH395_CHECK_TEST_DATA            0x65U
#define CH395_CHECK_EXPECTED             0x9AU

#define CH395_UART_INIT_BAUDRATE         9600U
#define CH395_UART_WORK_BAUDRATE         115200U

#define CH395_PROTO_TYPE_TCP             0x03U
#define CH395_PROTO_TYPE_UDP             0x02U

#define CH395_TCP_CLOSED                 0x00U
#define CH395_TCP_ESTABLISHED            0x04U

/* socket n 中断位：1 表示发送缓冲区空闲，2 表示发送成功。 */
#define CH395_SOCKET_INT_SEND_BUF_FREE   0x01U
#define CH395_SOCKET_INT_SEND_OK         0x02U
#define CH395_SOCKET_INT_RECV             0x04U
#define CH395_SOCKET_INT_CONNECT         0x08U
#define CH395_SOCKET_INT_DISCONNECT      0x10U
#define CH395_SOCKET_INT_TIMEOUT         0x40U

#define CH395_PROBE_SOCKET_INDEX         2U
#define CH395_COMMAND_TIMEOUT_MS         1000U
#define CH395_TCP_CONNECT_TIMEOUT_MS     5000U

#endif
