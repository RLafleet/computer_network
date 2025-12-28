#ifndef DNS_RESOLVER_H
#define DNS_RESOLVER_H

#include <stddef.h>
#include <stdint.h>

#include "dns_cache.h"
#include "dns_packet.h"

typedef struct {
    dns_record_t records[DNS_MAX_RECORDS];
    size_t count;
} dns_result_t;

int resolve_iterative(const char *qname, uint16_t qtype, dns_cache_t *cache,
                      dns_result_t *result, int debug,
                      char *err, size_t err_len);

#endif
