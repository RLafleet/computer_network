#ifndef SMTP_UTILS_H
#define SMTP_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#if defined(USE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define BUFFER_SIZE 1024
#define MAX_EMAIL_SIZE 8192
#define MAX_ATTACHMENTS 10

typedef struct {
    char* server;
    int port;
    char* sender;
    char* recipient;
    char* subject;
    char* body;
    char* body_file;
    char* attachments[MAX_ATTACHMENTS];
    int attachment_count;
    char* login;
    char* password;
    int use_tls;
    int verbose;
} smtp_config_t;

typedef struct {
    int sockfd;
#ifdef USE_OPENSSL
    SSL* ssl;
    SSL_CTX* ctx;
#endif
    int use_tls;
    int verbose;
} smtp_connection_t;

int connect_to_server(smtp_config_t* config, smtp_connection_t* conn);
int read_response(smtp_connection_t* conn, char* buffer, size_t buffer_size);
int send_command(smtp_connection_t* conn, const char* command);
int handle_smtp_dialog(smtp_connection_t* conn, smtp_config_t* config);
void print_usage(const char* program_name);
void init_config(smtp_config_t* config);
void free_config(smtp_config_t* config);

#endif