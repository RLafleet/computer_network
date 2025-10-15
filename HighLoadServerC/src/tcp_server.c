#include "tcp_server.h"

#include <string.h>
#include <stdio.h>

int tcp_server_create(TcpServer *s)
{
    if (!s)
        return -1;
    s->sock = (socket_handle)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s->sock == INVALID_SOCKET)
        return -1;

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
    return (*outClient == INVALID_SOCKET) ? -1 : 0;
}

void tcp_server_close(TcpServer *s)
{
    if (!s)
        return;
    if (s->sock != INVALID_SOCKET)
    {
        socket_close(s->sock);
    }
}

int get_local_address(socket_handle s, char *out, size_t outSize)
{
    struct sockaddr_in addr;
    int len = sizeof(addr);
    if (getsockname(s, (struct sockaddr *)&addr, &len) != 0)
        return -1;
    DWORD sz = (DWORD)outSize;
    if (WSAAddressToStringA((LPSOCKADDR)&addr, sizeof(addr), NULL, out, &sz) != 0)
        return -1;
    return 0;
}
