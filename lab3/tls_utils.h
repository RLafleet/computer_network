#ifndef TLS_UTILS_H
#define TLS_UTILS_H

#if defined(USE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "smtp_utils.h"

int init_openssl(void);
int cleanup_openssl(void);
int connect_with_starttls(smtp_connection_t* conn, const char* server, int port);
int starttls_handshake(smtp_connection_t* conn);
int authenticate_user(smtp_connection_t* conn, const char* username, const char* password);

#endif