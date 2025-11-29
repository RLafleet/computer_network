#include "tcp_server.h"
#include "tcp_client.h"
#include "proto.h"
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <windows.h>

static const int SERVER_NUMBER = 50;

typedef struct
{
    socket_handle client;
    char server_name[256];
} client_thread_data_t;

static volatile BOOL server_running = TRUE;

static BOOL WINAPI console_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT)
    {
        printf("\nReceive shutdown signal. Stop server\n");
        server_running = FALSE;
        return TRUE;
    }
    return FALSE;
}

static DWORD WINAPI handle_client_thread(LPVOID lpParam)
{
    client_thread_data_t *data = (client_thread_data_t *)lpParam;
    socket_handle client = data->client;
    const char *server_name = data->server_name;

    if (socket_set_timeout(client, 10) != 0)
    {
        printf("Fail to set timeout for client\n");
        socket_close(client);
        free(data);
        return 1;
    }

    printf("Process client connection with 10-s timer\n");

    char buf[1024];
    int r = socket_recv_some(client, buf, sizeof(buf));
    if (r <= 0)
    {
        if (r == 0)
        {
            printf("Client disconnect before sending data\n");
        }
        else
        {
            printf("Fail to receive data from client. Timeout or error\n");
        }
        socket_close(client);
        free(data);
        return 1;
    }

    Query q;
    if (parse_query(buf, (size_t)r, &q) != 0)
    {
        printf("Fail to parse query from client\n");
        socket_close(client);
        free(data);
        return 1;
    }

    if (q.number < 0 || q.number > 100)
    {
        printf("Invalid number %d. Server shutdown.\n", q.number);
        socket_close(client);
        server_running = FALSE;
        free(data);
        return 2;
    }

    print_info(q.name, server_name, q.number, SERVER_NUMBER);

    char out[256];
    Query resp;
    strncpy(resp.name, server_name, sizeof(resp.name) - 1);
    resp.name[sizeof(resp.name) - 1] = '\0';
    resp.number = SERVER_NUMBER;

    int w = construct_query(&resp, out, sizeof(out));
    if (w > 0)
    {
        if (socket_send_all(client, out, (size_t)w) != 0)
        {
            printf("Fail to send response to client\n");
        }
    }

    socket_close(client);
    free(data);
    return 0;
}

int run_server(unsigned short port, const char *name)
{
    if (!SetConsoleCtrlHandler(console_handler, TRUE))
    {
        printf("Fail to set console control handler\n");
        return -1;
    }

    TcpServer srv;
    if (tcp_server_create(&srv) != 0)
        return -1;
    if (tcp_server_bind_listen(&srv, port) != 0)
        return -1;

    char addr[128];
    if (get_local_address(srv.sock, addr, sizeof(addr)) == 0)
    {
        printf("Start server on %s\n", addr);
        printf("Press Ctrl+C to stop server\n\n");
    }

    while (server_running)
    {
        socket_handle client;
        if (tcp_server_accept(&srv, &client) != 0)
        {
            if (GetLastError() == WSAEINTR)
            {
                continue;
            }
            printf("Fail to accept client connection\n");
            continue;
        }

        printf("New client connect. Create a thread for him\n");

        client_thread_data_t *data = (client_thread_data_t *)malloc(sizeof(client_thread_data_t));
        if (!data)
        {
            printf("Fail to allocate memory for thread data\n");
            socket_close(client);
            continue;
        }

        data->client = client;
        strncpy(data->server_name, name, sizeof(data->server_name) - 1);
        data->server_name[sizeof(data->server_name) - 1] = '\0';

        HANDLE thread = CreateThread(NULL, 0, handle_client_thread, data, 0, NULL);
        if (thread == NULL)
        {
            printf("Fail to create thread: %d\n", GetLastError());
            socket_close(client);
            free(data);
            continue;
        }

        CloseHandle(thread);
    }

    printf("Server shut down\n");
    tcp_server_close(&srv);
    return 0;
}