#include "socket.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

int socket_init()
{
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (result != 0)
    {
        printf("WSAStartup fail. Err: %d\n", result);
    }
    return result;
}

void socket_cleanup()
{
    WSACleanup();
}

int socket_close(socket_handle s)
{
    if (s == INVALID_SOCKET)
    {
        return 0;
    }
    int result = closesocket(s);
    if (result != 0)
    {
        printf("Fail to close socket. Err: %d\n", WSAGetLastError());
    }
    return result;
}

int socket_send_all(socket_handle s, const char *data, size_t len)
{
    if (s == INVALID_SOCKET || !data || len == 0)
    {
        printf("Invalid parametr for socket_send_all\n");
        return -1;
    }

    const char *p = data;
    size_t left = len;
    while (left > 0)
    {
        int sent = send(s, p, (int)left, 0);
        if (sent <= 0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                continue;
            }
            printf("Send fail. Err: %d\n", error);
            return -1;
        }
        p += sent;
        left -= (size_t)sent;
    }
    return 0;
}

int socket_recv_some(socket_handle s, char *buf, size_t len)
{
    if (s == INVALID_SOCKET || !buf || len == 0)
    {
        printf("Invalid parame for socket_recv_some\n");
        return -1;
    }

    int r = recv(s, buf, (int)len, 0);
    if (r < 0)
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            return 0;
        }
        printf("Receive fail. Err: %d\n", error);
        return -1;
    }
    else if (r == 0)
    {
        printf("Connection close\n");
        return 0;
    }
    return r;
}

int socket_set_timeout(socket_handle s, int timeout_seconds)
{
    if (s == INVALID_SOCKET)
    {
        return -1;
    }

    DWORD timeout = timeout_seconds * 1000;
    int result = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    if (result != 0)
    {
        printf("Fail to set receive timeout: %d\n", WSAGetLastError());
        return -1;
    }

    result = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    if (result != 0)
    {
        printf("Fail to set send timeout: %d\n", WSAGetLastError());
        return -1;
    }

    return 0;
}
