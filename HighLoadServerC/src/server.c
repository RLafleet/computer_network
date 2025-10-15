#include "tcp_server.h"
#include "tcp_client.h"
#include "proto.h"
#include <stdio.h>
#include <string.h>

static const int SERVER_NUMBER = 50;

int run_server(unsigned short port, const char *name)
{
    TcpServer srv;
    if (tcp_server_create(&srv) != 0)
        return -1;
    if (tcp_server_bind_listen(&srv, port) != 0)
        return -1;

    char addr[128];
    if (get_local_address(srv.sock, addr, sizeof(addr)) == 0)
    {
        printf("Starting server on %s\n\n", addr);
    }

    for (;;)
    {
        socket_handle client;
        if (tcp_server_accept(&srv, &client) != 0)
            continue;

        char buf[1024];
        int r = socket_recv_some(client, buf, sizeof(buf));
        if (r <= 0)
        {
            socket_close(client);
            continue;
        }

        Query q;
        if (parse_query(buf, (size_t)r, &q) != 0)
        {
            socket_close(client);
            continue;
        }

        if (q.number < 0 || q.number > 100)
        {
            socket_close(client);
            continue;
        }

        print_info(q.name, name, q.number, SERVER_NUMBER);

        char out[256];
        Query resp;
        strncpy(resp.name, name, sizeof(resp.name) - 1);
        resp.name[sizeof(resp.name) - 1] = '\0';
        resp.number = SERVER_NUMBER;
        int w = construct_query(&resp, out, sizeof(out));
        if (w > 0)
        {
            socket_send_all(client, out, (size_t)w);
        }

        socket_close(client);
    }
}
