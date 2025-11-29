#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

#define MAX_METHOD_LEN 16
#define MAX_URI_LEN 256
#define MAX_VERSION_LEN 16
#define MAX_HOST_LEN 128

typedef struct HttpRequest
{
    char method[MAX_METHOD_LEN];
    char uri[MAX_URI_LEN];
    char version[MAX_VERSION_LEN];
    char host[MAX_HOST_LEN];
} HttpRequest;

int parse_http_request(const char *request_str, size_t request_len, HttpRequest *req);
int extract_filename_from_uri(const char *uri, char *filename, size_t filename_size);

#endif // HTTP_PARSER_H