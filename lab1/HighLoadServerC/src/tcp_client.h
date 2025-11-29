#pragma once
#include "socket.h"

typedef struct TcpClient {
	socket_handle sock;
} TcpClient;

int tcp_client_create(TcpClient* c);
int tcp_client_connect(TcpClient* c, const char* ip, unsigned short port);
int tcp_client_send(TcpClient* c, const char* data, size_t len);
int tcp_client_recv(TcpClient* c, char* buf, size_t len);
void tcp_client_close(TcpClient* c);

