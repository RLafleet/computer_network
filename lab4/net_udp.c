#include "net_udp.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int dns_udp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms) {
    if (!addr || !query || !response || !response_len) {
        return -1;
    }

    int sock = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, addr, addrlen) != 0) {
        close(sock);
        return -1;
    }

    ssize_t sent = send(sock, query, query_len, 0);
    if (sent < 0 || (size_t)sent != query_len) {
        close(sock);
        return -1;
    }

    ssize_t recvd = recv(sock, response, *response_len, 0);
    if (recvd < 0) {
        close(sock);
        return -1;
    }
    *response_len = (size_t)recvd;
    close(sock);
    return 0;
}
