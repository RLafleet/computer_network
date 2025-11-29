#include "http_server.h"
#include "socket.h"
#include "http_parser.h"
#include "file_handler.h"
#include "mime_types.h"
#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <process.h>

static volatile int server_running = 1;

static void signal_handler(int sig)
{
    printf("\nReceived signal %d. Shutting down server...\n", sig);
    server_running = 0;
}

int http_server_create(HttpServer *server, int port, const char *root_dir)
{
    if (!server || !root_dir)
        return -1;

    memset(server, 0, sizeof(HttpServer));
    server->port = port;
    strncpy(server->root_dir, root_dir, sizeof(server->root_dir) - 1);
    server->root_dir[sizeof(server->root_dir) - 1] = '\0';
    server->running = 0;

    return 0;
}

int http_server_start(HttpServer *server)
{
    if (!server)
        return -1;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (socket_create(&server->sock) != 0)
    {
        printf("Failed to create socket\n");
        return -1;
    }

    int retries = 3;
    int retry_delay = 1;

    while (retries > 0)
    {
        if (socket_bind_listen(server->sock, server->port) == 0)
        {
            break;
        }

        retries--;
        if (retries > 0)
        {
            printf("Failed to bind socket, retrying in %d second(s)... (%d retries left)\n",
                   retry_delay, retries);
            Sleep(retry_delay * 1000);
        }
    }

    if (retries == 0)
    {
        printf("Failed to bind socket to port %d after %d attempts\n", server->port, 3);
        socket_close(server->sock);
        return -1;
    }

    printf("Server listening on port %d\n", server->port);
    server->running = 1;

    ThreadPool *pool = thread_pool_create(10);
    if (!pool)
    {
        printf("Failed to create thread pool\n");
        socket_close(server->sock);
        return -1;
    }

    printf("Thread pool created with 10 worker threads\n");

    while (server_running)
    {
        socket_handle client_sock;
        if (socket_accept(server->sock, &client_sock) == 0)
        {
            ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
            if (client_data)
            {
                client_data->client_sock = client_sock;
                strncpy(client_data->root_dir, server->root_dir, sizeof(client_data->root_dir) - 1);
                client_data->root_dir[sizeof(client_data->root_dir) - 1] = '\0';

                if (thread_pool_add_task(pool, handle_client, client_data) != 0)
                {
                    printf("Failed to add task to thread pool\n");
                    free(client_data);
                    socket_close(client_sock);
                }
            }
            else
            {
                printf("Failed to allocate memory for client data\n");
                socket_close(client_sock);
            }
        }
    }

    thread_pool_destroy(pool);

    return 0;
}

int http_server_stop(HttpServer *server)
{
    if (!server)
        return -1;

    server->running = 0;
    server_running = 0;
    return 0;
}

void http_server_close(HttpServer *server)
{
    if (!server)
        return;

    if (server->sock != INVALID_SOCKET)
    {
        socket_close(server->sock);
        server->sock = INVALID_SOCKET;
    }
}

int http_handle_request(SOCKET client_sock, const char *root_dir)
{
    char buffer[4096];
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0)
    {
        printf("Failed to receive data from client\n");
        return -1;
    }

    buffer[bytes_received] = '\0';
    printf("Received request:\n%s\n", buffer);

    HttpRequest req;
    if (parse_http_request(buffer, bytes_received, &req) != 0)
    {
        printf("Failed to parse HTTP request\n");
        const char *response = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 11\r\n"
                               "\r\n"
                               "Bad Request";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    if (strcmp(req.method, "GET") != 0)
    {
        printf("Unsupported HTTP method: %s\n", req.method);
        const char *response = "HTTP/1.1 405 Method Not Allowed\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 18\r\n"
                               "\r\n"
                               "Method Not Allowed";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    char filename[256];
    if (extract_filename_from_uri(req.uri, filename, sizeof(filename)) != 0)
    {
        printf("Failed to extract filename from URI: %s\n", req.uri);
        const char *response = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 11\r\n"
                               "\r\n"
                               "Bad Request";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    printf("Requested URI: %s, Extracted filename: '%s'\n", req.uri, filename);

    if (strcmp(filename, "") == 0 || strcmp(filename, "/") == 0)
    {
        strncpy(filename, "index.html", sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }

    printf("Final filename to serve: '%s'\n", filename);

    char resolved_path[512];
    if (resolve_filepath(root_dir, filename, resolved_path, sizeof(resolved_path)) != 0)
    {
        printf("Failed to resolve file path for: %s\n", filename);
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 14\r\n"
                               "\r\n"
                               "File Not Found";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    printf("Root directory: %s\n", root_dir);
    printf("Resolved path: %s\n", resolved_path);

    FileInfo file_info;
    if (get_file_info(resolved_path, &file_info) != 0 || !file_info.exists)
    {
        printf("File not found: %s\n", resolved_path);
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 14\r\n"
                               "\r\n"
                               "File Not Found";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    printf("File exists: %s, Size: %zu bytes\n", resolved_path, file_info.size);

    if (!is_safe_path(root_dir, resolved_path))
    {
        printf("Unsafe file path access attempt: %s\n", resolved_path);
        const char *response = "HTTP/1.1 403 Forbidden\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 9\r\n"
                               "\r\n"
                               "Forbidden";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    char *file_content = NULL;
    size_t file_size = 0;
    if (read_file_content(resolved_path, &file_content, &file_size) != 0)
    {
        printf("Failed to read file: %s\n", resolved_path);
        const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 21\r\n"
                               "\r\n"
                               "Internal Server Error";
        send(client_sock, response, (int)strlen(response), 0);
        return -1;
    }

    const char *mime_type = get_mime_type(resolved_path);

    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n",
                              mime_type, file_size);

    if (header_len > 0 && header_len < (int)sizeof(header))
    {
        send(client_sock, header, header_len, 0);
        send(client_sock, file_content, (int)file_size, 0);
    }

    free(file_content);

    printf("Served file: %s (%zu bytes)\n", resolved_path, file_size);
    return 0;
}