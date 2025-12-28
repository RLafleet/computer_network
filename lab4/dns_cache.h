#ifndef DNS_CACHE_H
#define DNS_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "dns_packet.h"

typedef struct {
    dns_record_t record;
    time_t expires_at;
} cache_record_t;

typedef struct cache_entry {
    char name[DNS_MAX_NAME];
    uint16_t type;
    cache_record_t records[DNS_MAX_RECORDS];
    size_t record_count;
    struct cache_entry *next;
} cache_entry_t;

typedef struct {
    cache_entry_t *head;
} dns_cache_t;

void cache_init(dns_cache_t *cache);
void cache_free(dns_cache_t *cache);
int cache_get(dns_cache_t *cache, const char *name, uint16_t type,
              dns_record_t *out, size_t max_out, size_t *out_count);
void cache_put(dns_cache_t *cache, const char *name, uint16_t type,
               const dns_record_t *records, size_t record_count);

#endif
