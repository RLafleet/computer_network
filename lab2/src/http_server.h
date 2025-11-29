#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stddef.h>
#include "http_parser.h"

typedef struct HttpServer
{
    SOCKET sock;
    int port;
    char root_dir[256];
    int running;
} HttpServer;

typedef struct HttpResponse
{
    int status_code;
    char status_text[32];
    char content_type[64];
    size_t content_length;
    char *body;
} HttpResponse;

int http_server_create(HttpServer *server, int port, const char *root_dir);
int http_server_start(HttpServer *server);
int http_server_stop(HttpServer *server);
void http_server_close(HttpServer *server);

int http_handle_request(SOCKET client_sock, const char *root_dir);

#endif // HTTP_SERVER_H