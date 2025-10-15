#pragma once

#include <stddef.h>

typedef struct Query
{
    char name[256];
    int number;
} Query;

int construct_query(const Query *q, char *outBuf, size_t outBufSize);
int parse_query(const char *input, size_t len, Query *out);
void print_info(const char *clientName, const char *serverName, int clientNumber, int serverNumber);
