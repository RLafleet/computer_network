#include "proto.h"
#include <stdio.h>
#include <string.h>

int construct_query(const Query *q, char *outBuf, size_t outBufSize)
{
    if (!q || !outBuf || outBufSize == 0)
        return -1;

    int written = snprintf(outBuf, outBufSize, "%s\n%d\n", q->name, q->number);
    if (written < 0 || (size_t)written >= outBufSize)
        return -1;

    return written;
}

int parse_query(const char *input, size_t len, Query *out)
{
    if (!input || !out)
        return -1;

    const char *nl = memchr(input, '\n', len);
    if (!nl)
        return -1;

    size_t nameLen = (size_t)(nl - input);
    if (nameLen >= sizeof(out->name))
        return -1;

    memcpy(out->name, input, nameLen);
    out->name[nameLen] = '\0';

    const char *numStart = nl + 1;
    const char *end = (const char *)memchr(numStart, '\n', (size_t)(input + len - numStart));
    size_t numLen = end ? (size_t)(end - numStart) : (size_t)(input + len - numStart);
    char tmp[64];
    if (numLen >= sizeof(tmp))
        return -1;

    memcpy(tmp, numStart, numLen);
    tmp[numLen] = '\0';

    int number = 0;
    if (sscanf(tmp, "%d", &number) != 1)
        return -1;

    out->number = number;
    return 0;
}

void print_info(const char *clientName, const char *serverName, int clientNumber, int serverNumber)
{
    printf("Client: %s\n", clientName);
    printf("Server: %s\n", serverName);
    printf("Client number: %d\n", clientNumber);
    printf("Server number: %d\n", serverNumber);
    printf("Sum: %d\n\n", clientNumber + serverNumber);
}
