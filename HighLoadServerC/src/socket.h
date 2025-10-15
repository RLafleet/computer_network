#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_handle;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_handle;
#endif

#include <stddef.h>

int socket_init();
void socket_cleanup();

int socket_close(socket_handle s);
int socket_send_all(socket_handle s, const char *data, size_t len);
int socket_recv_some(socket_handle s, char *buf, size_t len);
