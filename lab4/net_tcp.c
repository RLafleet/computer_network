#include "net_tcp.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int dns_tcp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms) {
    if (!addr || !query || !response || !response_len) {
        return -1;
    }

    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
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

    uint16_t net_len = htons((uint16_t)query_len);
    if (send_all(sock, (const uint8_t *)&net_len, sizeof(net_len)) != 0) {
        close(sock);
        return -1;
    }
    if (send_all(sock, query, query_len) != 0) {
        close(sock);
        return -1;
    }

    uint16_t resp_len = 0;
    if (recv_all(sock, (uint8_t *)&resp_len, sizeof(resp_len)) != 0) {
        close(sock);
        return -1;
    }
    resp_len = ntohs(resp_len);
    if (resp_len == 0 || resp_len > *response_len) {
        close(sock);
        return -1;
    }

    if (recv_all(sock, response, resp_len) != 0) {
        close(sock);
        return -1;
    }
    *response_len = resp_len;

    close(sock);
    return 0;
}
