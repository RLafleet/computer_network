#include "tcp_client.h"
#include "proto.h"
#include <stdio.h>
#include <string.h>

int run_client(const char *ip, unsigned short port, const char *name)
{
    TcpClient c;
    if (tcp_client_create(&c) != 0)
        return -1;
    if (tcp_client_connect(&c, ip, port) != 0)
        return -1;

    int number = 0;
    printf("Enter number: ");
    if (scanf("%d", &number) != 1)
        return -1;

    Query q;
    strncpy(q.name, name, sizeof(q.name) - 1);
    q.name[sizeof(q.name) - 1] = '\0';
    q.number = number;

    char out[256];
    int w = construct_query(&q, out, sizeof(out));
    if (w <= 0)
        return -1;
    if (tcp_client_send(&c, out, (size_t)w) != 0)
        return -1;

    char in[256];
    int r = tcp_client_recv(&c, in, sizeof(in));
    if (r <= 0)
        return -1;

    Query resp;
    if (parse_query(in, (size_t)r, &resp) != 0)
        return -1;
    print_info(name, resp.name, number, resp.number);
    return 0;
}
