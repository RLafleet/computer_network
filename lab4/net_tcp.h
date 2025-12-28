#ifndef DNS_NET_TCP_H
#define DNS_NET_TCP_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

int dns_tcp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms);

#endif
