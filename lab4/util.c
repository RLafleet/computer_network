#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int send_all(int fd, const uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(fd, buf + total, len - total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        total += (size_t)sent;
    }
    return 0;
}

int recv_all(int fd, uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t recvd = recv(fd, buf + total, len - total, 0);
        if (recvd < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (recvd == 0) {
            return -1;
        }
        total += (size_t)recvd;
    }
    return 0;
}

int sockaddr_to_string(const struct sockaddr *addr, char *out, size_t out_len) {
    if (addr == NULL || out == NULL || out_len == 0) {
        return -1;
    }
    void *src = NULL;
    uint16_t port = 0;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        src = (void *)&in->sin_addr;
        port = ntohs(in->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)addr;
        src = (void *)&in6->sin6_addr;
        port = ntohs(in6->sin6_port);
    } else {
        return -1;
    }

    char ip[INET6_ADDRSTRLEN];
    if (inet_ntop(addr->sa_family, src, ip, sizeof(ip)) == NULL) {
        return -1;
    }
    if (snprintf(out, out_len, "%s:%u", ip, port) >= (int)out_len) {
        return -1;
    }
    return 0;
}

int make_sockaddr(const char *ip, uint16_t port,
                  struct sockaddr_storage *out, socklen_t *out_len) {
    if (ip == NULL || out == NULL || out_len == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    struct sockaddr_in in;
    if (inet_pton(AF_INET, ip, &in.sin_addr) == 1) {
        in.sin_family = AF_INET;
        in.sin_port = htons(port);
        memcpy(out, &in, sizeof(in));
        *out_len = sizeof(in);
        return 0;
    }

    struct sockaddr_in6 in6;
    if (inet_pton(AF_INET6, ip, &in6.sin6_addr) == 1) {
        in6.sin6_family = AF_INET6;
        in6.sin6_port = htons(port);
        memcpy(out, &in6, sizeof(in6));
        *out_len = sizeof(in6);
        return 0;
    }
    return -1;
}
