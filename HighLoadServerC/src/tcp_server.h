#pragma once
#include "socket.h"

typedef struct TcpServer {
	socket_handle sock;
} TcpServer;

int tcp_server_create(TcpServer* s);
int tcp_server_bind_listen(TcpServer* s, unsigned short port);
int tcp_server_accept(TcpServer* s, socket_handle* outClient);
void tcp_server_close(TcpServer* s);

int get_local_address(socket_handle s, char* out, size_t outSize);

