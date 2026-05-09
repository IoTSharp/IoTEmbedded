#ifndef CH395_PROBE_H
#define CH395_PROBE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ch395_probe_tcp_port(const char *host, uint16_t port);
bool ch395_parse_ipv4(const char *host, uint8_t ip[4]);

#ifdef __cplusplus
}
#endif

#endif
