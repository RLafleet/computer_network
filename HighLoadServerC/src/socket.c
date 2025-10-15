#include "socket.h"

#ifdef _WIN32
#include <stdio.h>
int socket_init()
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}
void socket_cleanup() { WSACleanup(); }
int socket_close(socket_handle s) { return closesocket(s); }
#else
int socket_init() { return 0; }
void socket_cleanup() {}
int socket_close(socket_handle s) { return close(s); }
#endif

#include <string.h>

int socket_send_all(socket_handle s, const char *data, size_t len)
{
    const char *p = data;
    size_t left = len;
    while (left > 0)
    {
#ifdef _WIN32
        int sent = send(s, p, (int)left, 0);
#else
        ssize_t sent = send(s, p, left, 0);
#endif
        if (sent <= 0)
            return -1;
        p += sent;
        left -= (size_t)sent;
    }
    return 0;
}

int socket_recv_some(socket_handle s, char *buf, size_t len)
{
#ifdef _WIN32
    int r = recv(s, buf, (int)len, 0);
#else
    ssize_t r = recv(s, buf, len, 0);
#endif
    return (int)r;
}
