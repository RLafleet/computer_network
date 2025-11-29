#ifndef SOCKET_H
#define SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stddef.h>

typedef SOCKET socket_handle;

int socket_init();
void socket_cleanup();
int socket_create(socket_handle *s);
int socket_bind_listen(socket_handle s, int port);
int socket_accept(socket_handle s, socket_handle *outClient);
int socket_close(socket_handle s);
int socket_send_all(socket_handle s, const char *data, size_t len);
int socket_recv_some(socket_handle s, char *buf, size_t len);
int socket_set_timeout(socket_handle s, int timeout_seconds);

#endif // SOCKET_H