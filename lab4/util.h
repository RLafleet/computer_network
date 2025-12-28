#ifndef DNS_UTIL_H
#define DNS_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

int send_all(int fd, const uint8_t *buf, size_t len);
int recv_all(int fd, uint8_t *buf, size_t len);

int sockaddr_to_string(const struct sockaddr *addr, char *out, size_t out_len);
int make_sockaddr(const char *ip, uint16_t port,
                  struct sockaddr_storage *out, socklen_t *out_len);

#endif
