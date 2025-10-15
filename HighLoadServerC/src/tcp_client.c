#include "tcp_client.h"

#include <string.h>
#include <stdio.h>

int tcp_client_create(TcpClient *c)
{
    if (!c)
        return -1;
    c->sock = (socket_handle)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (c->sock == INVALID_SOCKET)
        return -1;
#else
    if (c->sock < 0)
        return -1;
#endif
    printf("Client socket created\n");
    return 0;
}

int tcp_client_connect(TcpClient *c, const char *ip, unsigned short port)
{
    if (!c || !ip)
        return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    int rc = connect(c->sock, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == 0)
    {
        printf("Client socket connected to %s:%u\n", ip, (unsigned)port);
    }
    return rc;
}

int tcp_client_send(TcpClient *c, const char *data, size_t len)
{
    if (!c || !data)
        return -1;
    printf("Send: %.*s\n\n", (int)len, data);
    return socket_send_all(c->sock, data, len);
}

int tcp_client_recv(TcpClient *c, char *buf, size_t len)
{
    if (!c || !buf || len == 0)
        return -1;
    int r = socket_recv_some(c->sock, buf, len);
    if (r > 0)
    {
        printf("Receive: %.*s\n\n", r, buf);
    }
    return r;
}

void tcp_client_close(TcpClient *c)
{
    if (!c)
        return;
#ifdef _WIN32
    if (c->sock != INVALID_SOCKET)
#else
    if (c->sock >= 0)
#endif
    {
        socket_close(c->sock);
        printf("Connection closed\n");
    }
}
