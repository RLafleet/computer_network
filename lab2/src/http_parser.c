#include "http_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int parse_http_request(const char *request_str, size_t request_len, HttpRequest *req)
{
    if (!request_str || !req || request_len == 0)
        return -1;

    memset(req, 0, sizeof(HttpRequest));

    const char *first_line_end = strchr(request_str, '\r');
    if (!first_line_end)
        return -1;

    size_t first_line_len = first_line_end - request_str;
    if (first_line_len >= 512)
        return -1;

    char first_line[512];
    strncpy(first_line, request_str, first_line_len);
    first_line[first_line_len] = '\0';

    char *token = strtok(first_line, " ");
    if (!token)
        return -1;

    strncpy(req->method, token, sizeof(req->method) - 1);
    req->method[sizeof(req->method) - 1] = '\0';

    token = strtok(NULL, " ");
    if (!token)
        return -1;

    strncpy(req->uri, token, sizeof(req->uri) - 1);
    req->uri[sizeof(req->uri) - 1] = '\0';

    token = strtok(NULL, " ");
    if (!token)
        return -1;

    strncpy(req->version, token, sizeof(req->version) - 1);
    req->version[sizeof(req->version) - 1] = '\0';

    const char *headers_start = strstr(request_str, "\r\n");
    if (headers_start)
    {
        headers_start += 2;

        const char *host_header = strstr(headers_start, "Host:");
        if (host_header)
        {
            const char *host_line_end = strstr(host_header, "\r\n");
            if (host_line_end)
            {
                const char *host_value = host_header + 5;
                while (*host_value == ' ' || *host_value == '\t')
                    host_value++;

                size_t host_len = host_line_end - host_value;
                if (host_len > 0 && host_len < sizeof(req->host))
                {
                    strncpy(req->host, host_value, host_len);
                    req->host[host_len] = '\0';
                }
            }
        }
    }

    return 0;
}

int extract_filename_from_uri(const char *uri, char *filename, size_t filename_size)
{
    if (!uri || !filename || filename_size == 0)
        return -1;

    if (strcmp(uri, "/") == 0)
    {
        filename[0] = '\0';
        return 0;
    }

    const char *path_start = uri;
    if (path_start[0] == '/')
        path_start++;

    size_t path_len = strlen(path_start);
    if (path_len >= filename_size)
        return -1;

    size_t out_pos = 0;
    for (size_t i = 0; i < path_len && out_pos < filename_size - 1;)
    {
        if (path_start[i] == '%' && i + 2 < path_len)
        {
            char hex[3] = {path_start[i + 1], path_start[i + 2], '\0'};
            char decoded = (char)strtol(hex, NULL, 16);
            filename[out_pos++] = decoded;
            i += 3;
        }
        else
        {
            filename[out_pos++] = path_start[i++];
        }
    }
    filename[out_pos] = '\0';

    return 0;
}