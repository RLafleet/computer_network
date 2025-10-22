#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run_server(unsigned short port, const char *name);
int run_client(const char *ip, unsigned short port, const char *name);

typedef struct Args
{
    int isServer;
    int port;
    char address[256];
    char name[256];
} Args;

static int parse_args(int argc, char **argv, Args *out)
{
    if (!out)
        return 0;
    if (argc == 3)
    {
        out->isServer = 1;
        out->port = atoi(argv[1]);
        strncpy(out->name, argv[2], sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return 1;
    }
    if (argc == 4)
    {
        out->isServer = 0;
        strncpy(out->address, argv[1], sizeof(out->address) - 1);
        out->address[sizeof(out->address) - 1] = '\0';
        out->port = atoi(argv[2]);
        strncpy(out->name, argv[3], sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    Args args;
    if (!parse_args(argc, argv, &args))
    {
        printf("Server mode: HighLoadServerC <port> <name>\n\n  Client mode: HighLoadServerC <address> <port> <name>\n\n");
        return EXIT_FAILURE;
    }

    if (socket_init() != 0)
    {
        return EXIT_FAILURE;
    }

    int rc = 0;
    if (args.isServer)
    {
        rc = run_server((unsigned short)args.port, args.name);
    }
    else
    {
        rc = run_client(args.address, (unsigned short)args.port, args.name);
    }

    socket_cleanup();
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
