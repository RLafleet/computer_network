#include "dns_cache.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

void cache_init(dns_cache_t *cache) {
    if (cache) {
        cache->head = NULL;
    }
}

static void free_entry(cache_entry_t *entry) {
    if (entry) {
        free(entry);
    }
}

void cache_free(dns_cache_t *cache) {
    if (!cache) {
        return;
    }
    cache_entry_t *cur = cache->head;
    while (cur) {
        cache_entry_t *next = cur->next;
        free_entry(cur);
        cur = next;
    }
    cache->head = NULL;
}

static void normalize_key(const char *name, char *out, size_t out_len) {
    dns_normalize_name(name, out, out_len);
}

int cache_get(dns_cache_t *cache, const char *name, uint16_t type,
              dns_record_t *out, size_t max_out, size_t *out_count) {
    if (!cache || !name || !out || !out_count) {
        return -1;
    }

    char key[DNS_MAX_NAME];
    normalize_key(name, key, sizeof(key));
    time_t now = time(NULL);

    cache_entry_t *prev = NULL;
    cache_entry_t *cur = cache->head;
    while (cur) {
        if (cur->type == type && strcasecmp(cur->name, key) == 0) {
            size_t valid_count = 0;
            for (size_t i = 0; i < cur->record_count; i++) {
                if (cur->records[i].expires_at > now) {
                    if (valid_count < max_out) {
                        out[valid_count] = cur->records[i].record;
                        valid_count++;
                    }
                }
            }
            if (valid_count > 0) {
                *out_count = valid_count;
                return 0;
            }
            if (prev) {
                prev->next = cur->next;
            } else {
                cache->head = cur->next;
            }
            free_entry(cur);
            return -1;
        }
        prev = cur;
        cur = cur->next;
    }

    return -1;
}

void cache_put(dns_cache_t *cache, const char *name, uint16_t type,
               const dns_record_t *records, size_t record_count) {
    if (!cache || !name || !records || record_count == 0) {
        return;
    }

    char key[DNS_MAX_NAME];
    normalize_key(name, key, sizeof(key));

    cache_entry_t *cur = cache->head;
    cache_entry_t *prev = NULL;
    while (cur) {
        if (cur->type == type && strcasecmp(cur->name, key) == 0) {
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    if (!cur) {
        cur = (cache_entry_t *)calloc(1, sizeof(cache_entry_t));
        if (!cur) {
            return;
        }
        strncpy(cur->name, key, sizeof(cur->name) - 1);
        cur->name[sizeof(cur->name) - 1] = '\0';
        cur->type = type;
        cur->next = NULL;
        if (prev) {
            prev->next = cur;
        } else {
            cache->head = cur;
        }
    }

    size_t count = record_count > DNS_MAX_RECORDS ? DNS_MAX_RECORDS : record_count;
    time_t now = time(NULL);
    for (size_t i = 0; i < count; i++) {
        cur->records[i].record = records[i];
        cur->records[i].expires_at = now + (time_t)records[i].ttl;
    }
    cur->record_count = count;
}
