#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dns_cache.h"
#include "dns_packet.h"
#include "resolver.h"

static void usage(const char *prog) {
    fprintf(stderr, "Использование: %s <домен> <тип> [-d]\n", prog);
}

int main(int argc, char **argv) {
    const char *domain = NULL;
    const char *type_str = NULL;
    int debug = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
        } else if (!domain) {
            domain = argv[i];
        } else if (!type_str) {
            type_str = argv[i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (!domain || !type_str) {
        usage(argv[0]);
        return 1;
    }

    uint16_t qtype = 0;
    if (dns_type_from_string(type_str, &qtype) != 0) {
        fprintf(stderr, "Неподдерживаемый тип: %s\n", type_str);
        return 1;
    }

    srand((unsigned int)time(NULL));

    dns_cache_t cache;
    cache_init(&cache);

    dns_result_t result;
    char err[256];
    if (resolve_iterative(domain, qtype, &cache, &result, debug, err, sizeof(err)) != 0) {
        fprintf(stderr, "Ошибка разрешения: %s\n", err);
        cache_free(&cache);
        return 1;
    }

    for (size_t i = 0; i < result.count; i++) {
        const dns_record_t *rec = &result.records[i];
        if (rec->type == DNS_TYPE_MX && rec->has_preference) {
            printf("%u %s\n", rec->preference, rec->data);
        } else {
            printf("%s\n", rec->data);
        }
    }

    cache_free(&cache);
    return 0;
}
