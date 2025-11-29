#include "socket.h"
#include <stdio.h>
#include <string.h>

int socket_init()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void socket_cleanup()
{
    WSACleanup();
}

int socket_create(socket_handle *s)
{
    if (!s)
        return -1;

    *s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return (*s == INVALID_SOCKET) ? -1 : 0;
}

int socket_bind_listen(socket_handle s, int port)
{
    if (s == INVALID_SOCKET)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) != 0)
    {
        printf("Warning: Failed to set SO_REUSEADDR option\n");
    }

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("bind");
        return -1;
    }

    printf("Server socket bind to %d\n", port);
    if (listen(s, 16) != 0)
    {
        perror("listen");
        return -1;
    }

    return 0;
}

int socket_accept(socket_handle s, socket_handle *outClient)
{
    if (s == INVALID_SOCKET || !outClient)
        return -1;

    *outClient = accept(s, NULL, NULL);
    return (*outClient == INVALID_SOCKET) ? -1 : 0;
}

int socket_close(socket_handle s)
{
    if (s == INVALID_SOCKET)
        return -1;

    return closesocket(s);
}

int socket_send_all(socket_handle s, const char *data, size_t len)
{
    if (s == INVALID_SOCKET || !data)
        return -1;

    size_t sent = 0;
    while (sent < len)
    {
        int n = send(s, data + sent, (int)(len - sent), 0);
        if (n <= 0)
            return -1;
        sent += n;
    }

    return 0;
}

int socket_recv_some(socket_handle s, char *buf, size_t len)
{
    if (s == INVALID_SOCKET || !buf || len == 0)
        return -1;

    int n = recv(s, buf, (int)len, 0);
    return n;
}

int socket_set_timeout(socket_handle s, int timeout_seconds)
{
    if (s == INVALID_SOCKET)
        return -1;

    DWORD timeout_ms = timeout_seconds * 1000;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms)) != 0)
        return -1;

    if (setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms)) != 0)
        return -1;

    return 0;
}