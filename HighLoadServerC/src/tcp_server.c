#include "tcp_server.h"

#include <string.h>
#include <stdio.h>

int tcp_server_create(TcpServer *s)
{
    if (!s)
        return -1;
    s->sock = (socket_handle)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s->sock == (socket_handle)-1
#ifdef _WIN32
        || s->sock == INVALID_SOCKET
#endif
    )
        return -1;

#ifndef _WIN32
    int yes = 1;
    setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    printf("Server socket created\n");
    return 0;
}

int tcp_server_bind_listen(TcpServer *s, unsigned short port)
{
    if (!s)
        return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s->sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -1;
    printf("Server socket bind to %u\n", (unsigned)port);
    if (listen(s->sock, 16) != 0)
        return -1;
    return 0;
}

int tcp_server_accept(TcpServer *s, socket_handle *outClient)
{
    if (!s || !outClient)
        return -1;
    *outClient = accept(s->sock, NULL, NULL);
#ifdef _WIN32
    return (*outClient == INVALID_SOCKET) ? -1 : 0;
#else
    return (*outClient < 0) ? -1 : 0;
#endif
}

void tcp_server_close(TcpServer *s)
{
    if (!s)
        return;
    if (s->sock
#ifdef _WIN32
        != INVALID_SOCKET
#else
        >= 0
#endif
    )
    {
        socket_close(s->sock);
    }
}

int get_local_address(socket_handle s, char *out, size_t outSize)
{
    struct sockaddr_in addr;
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif
    if (getsockname(s, (struct sockaddr *)&addr, &len) != 0)
        return -1;
#ifdef _WIN32
    DWORD sz = (DWORD)outSize;
    if (WSAAddressToStringA((LPSOCKADDR)&addr, sizeof(addr), NULL, out, &sz) != 0)
        return -1;
    return 0;
#else
    const char *ip = inet_ntoa(addr.sin_addr);
    if (!ip)
        return -1;
    int w = snprintf(out, outSize, "%s:%u", ip, (unsigned)ntohs(addr.sin_port));
    return (w < 0 || (size_t)w >= outSize) ? -1 : 0;
#endif
}
