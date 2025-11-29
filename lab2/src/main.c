#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket.h"
#include "http_server.h"
#include <direct.h>
#define getcwd _getcwd

static int parse_args(int argc, char **argv, int *port, char *root_dir, size_t root_dir_size)
{
    if (argc != 3)
    {
        return 0;
    }

    *port = atoi(argv[1]);
    if (*port <= 1023)
    {
        printf("Port must be greater than 1023\n");
        return 0;
    }

    char abs_path[512];
    if (_fullpath(abs_path, argv[2], sizeof(abs_path)) != NULL)
    {
        strncpy(root_dir, abs_path, root_dir_size - 1);
        root_dir[root_dir_size - 1] = '\0';
    }
    else
    {
        char current_dir[512];
        if (getcwd(current_dir, sizeof(current_dir)) != NULL)
        {
            snprintf(abs_path, sizeof(abs_path), "%s\\%s", current_dir, argv[2]);
            strncpy(root_dir, abs_path, root_dir_size - 1);
            root_dir[root_dir_size - 1] = '\0';
        }
        else
        {
            strncpy(root_dir, argv[2], root_dir_size - 1);
            root_dir[root_dir_size - 1] = '\0';
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    int port = 0;
    char root_dir[256] = {0};

    if (!parse_args(argc, argv, &port, root_dir, sizeof(root_dir)))
    {
        printf("Usage: %s <port> <root_directory>\n", argv[0]);
        printf("Example: %s 8080 ./www\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (socket_init() != 0)
    {
        printf("Failed to initialize socket library\n");
        return EXIT_FAILURE;
    }

    HttpServer server;
    if (http_server_create(&server, port, root_dir) != 0)
    {
        printf("Failed to create HTTP server\n");
        socket_cleanup();
        return EXIT_FAILURE;
    }

    printf("Starting HTTP server on port %d, serving directory: %s\n", port, root_dir);
    printf("Press Ctrl+C to stop server\n");

    if (http_server_start(&server) != 0)
    {
        printf("Failed to start HTTP server\n");
        http_server_close(&server);
        socket_cleanup();
        return EXIT_FAILURE;
    }

    http_server_close(&server);
    socket_cleanup();
    return EXIT_SUCCESS;
}