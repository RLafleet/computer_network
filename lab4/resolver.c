#include "resolver.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "dns_packet.h"
#include "net_tcp.h"
#include "net_udp.h"
#include "util.h"

#define DNS_PORT 53
#define DNS_MAX_SERVERS 32
#define DNS_MAX_DEPTH 10
#define DNS_MAX_ITERATIONS 25
#define DNS_QUERY_TIMEOUT_MS 3000
#define DNS_MAX_PACKET 4096

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
    char name[DNS_MAX_NAME];
} dns_server_t;

typedef struct {
    dns_server_t servers[DNS_MAX_SERVERS];
    size_t count;
} dns_server_list_t;

static void debug_log(int debug, const char *fmt, ...) {
    if (!debug) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static int name_equals(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

static void init_root_servers(dns_server_list_t *list) {
    static const char *root_ips[] = {
        "198.41.0.4",
        "199.9.14.201",
        "192.33.4.12",
        "199.7.91.13",
        "192.203.230.10",
        "192.5.5.241",
        "192.112.36.4",
        "198.97.190.53",
        "192.36.148.17",
        "192.58.128.30",
        "193.0.14.129",
        "199.7.83.42",
        "202.12.27.33"
    };

    list->count = 0;
    for (size_t i = 0; i < sizeof(root_ips) / sizeof(root_ips[0]); i++) {
        dns_server_t *srv = &list->servers[list->count];
        if (make_sockaddr(root_ips[i], DNS_PORT, &srv->addr, &srv->addrlen) == 0) {
            strncpy(srv->name, root_ips[i], sizeof(srv->name) - 1);
            srv->name[sizeof(srv->name) - 1] = '\0';
            list->count++;
        }
        if (list->count >= DNS_MAX_SERVERS) {
            break;
        }
    }
}

static int query_server(const dns_server_t *server, const uint8_t *query,
                        size_t query_len, dns_message_t *response, int debug) {
    uint8_t buffer[DNS_MAX_PACKET];
    size_t buf_len = sizeof(buffer);

    char addr_str[INET6_ADDRSTRLEN + 8];
    if (sockaddr_to_string((const struct sockaddr *)&server->addr, addr_str, sizeof(addr_str)) != 0) {
        strncpy(addr_str, "<unknown>", sizeof(addr_str) - 1);
        addr_str[sizeof(addr_str) - 1] = '\0';
    }

    debug_log(debug, "Запрос UDP %s", addr_str);
    if (dns_udp_query((const struct sockaddr *)&server->addr, server->addrlen,
                      query, query_len, buffer, &buf_len, DNS_QUERY_TIMEOUT_MS) != 0) {
        debug_log(debug, "Не удалось выполнить UDP-запрос к %s", addr_str);
        return -1;
    }

    dns_message_t msg;
    if (dns_parse_response(buffer, buf_len, &msg) != 0) {
        debug_log(debug, "Не удалось разобрать UDP-ответ от %s", addr_str);
        return -1;
    }

    if (dns_is_tc(&msg)) {
        debug_log(debug, "Установлен флаг TC, повтор по TCP %s", addr_str);
        buf_len = sizeof(buffer);
        if (dns_tcp_query((const struct sockaddr *)&server->addr, server->addrlen,
                          query, query_len, buffer, &buf_len, DNS_QUERY_TIMEOUT_MS) != 0) {
            debug_log(debug, "Не удалось выполнить TCP-запрос к %s", addr_str);
            return -1;
        }
        if (dns_parse_response(buffer, buf_len, &msg) != 0) {
            debug_log(debug, "Не удалось разобрать TCP-ответ от %s", addr_str);
            return -1;
        }
    }

    *response = msg;
    debug_log(debug, "Ответ %s: rcode=%d, ответов=%u, авторитативных=%u, дополнительных=%u",
              addr_str, dns_rcode(&msg), msg.header.ancount, msg.header.nscount, msg.header.arcount);
    return 0;
}

static void add_server_from_record(dns_server_list_t *list, const dns_record_t *rec) {
    if (list->count >= DNS_MAX_SERVERS) {
        return;
    }
    dns_server_t *srv = &list->servers[list->count];
    if (make_sockaddr(rec->data, DNS_PORT, &srv->addr, &srv->addrlen) == 0) {
        strncpy(srv->name, rec->name, sizeof(srv->name) - 1);
        srv->name[sizeof(srv->name) - 1] = '\0';
        list->count++;
    }
}

static int collect_answer_records(const dns_message_t *msg, const char *qname,
                                  uint16_t qtype, dns_result_t *result) {
    result->count = 0;
    for (size_t i = 0; i < msg->answer_count && result->count < DNS_MAX_RECORDS; i++) {
        const dns_record_t *rec = &msg->answers[i];
        if (!name_equals(rec->name, qname)) {
            continue;
        }
        if (qtype == DNS_TYPE_ANY || rec->type == qtype) {
            result->records[result->count++] = *rec;
        }
    }
    return result->count > 0 ? 0 : -1;
}

static const dns_record_t *find_cname_record(const dns_message_t *msg, const char *qname) {
    for (size_t i = 0; i < msg->answer_count; i++) {
        const dns_record_t *rec = &msg->answers[i];
        if (rec->type == DNS_TYPE_CNAME && name_equals(rec->name, qname)) {
            return rec;
        }
    }
    return NULL;
}

static int collect_glue_servers(const dns_message_t *msg, const char ns_names[][DNS_MAX_NAME],
                                size_t ns_count, dns_server_list_t *out_list) {
    out_list->count = 0;
    for (size_t i = 0; i < msg->additional_count; i++) {
        const dns_record_t *rec = &msg->additionals[i];
        if (rec->type != DNS_TYPE_A && rec->type != DNS_TYPE_AAAA) {
            continue;
        }
        for (size_t j = 0; j < ns_count; j++) {
            if (name_equals(rec->name, ns_names[j])) {
                add_server_from_record(out_list, rec);
                break;
            }
        }
    }
    return out_list->count > 0 ? 0 : -1;
}

static int collect_ns_names(const dns_message_t *msg, char out[][DNS_MAX_NAME], size_t *out_count) {
    *out_count = 0;
    for (size_t i = 0; i < msg->authority_count && *out_count < DNS_MAX_RECORDS; i++) {
        const dns_record_t *rec = &msg->authorities[i];
        if (rec->type == DNS_TYPE_NS) {
            strncpy(out[*out_count], rec->data, DNS_MAX_NAME - 1);
            out[*out_count][DNS_MAX_NAME - 1] = '\0';
            (*out_count)++;
        }
    }
    return *out_count > 0 ? 0 : -1;
}

static int resolve_iterative_internal(const char *qname, uint16_t qtype, dns_cache_t *cache,
                                      dns_result_t *result, int debug, int depth,
                                      char *err, size_t err_len);

static int resolve_ns_addresses(const char *ns_name, dns_cache_t *cache,
                                dns_server_list_t *out_list, int debug, int depth) {
    dns_result_t tmp;
    char err[128];

    if (resolve_iterative_internal(ns_name, DNS_TYPE_A, cache, &tmp, debug, depth + 1, err, sizeof(err)) == 0) {
        for (size_t i = 0; i < tmp.count; i++) {
            if (tmp.records[i].type == DNS_TYPE_A) {
                add_server_from_record(out_list, &tmp.records[i]);
            }
        }
    }

    if (resolve_iterative_internal(ns_name, DNS_TYPE_AAAA, cache, &tmp, debug, depth + 1, err, sizeof(err)) == 0) {
        for (size_t i = 0; i < tmp.count; i++) {
            if (tmp.records[i].type == DNS_TYPE_AAAA) {
                add_server_from_record(out_list, &tmp.records[i]);
            }
        }
    }

    return out_list->count > 0 ? 0 : -1;
}

static int resolve_iterative_internal(const char *qname, uint16_t qtype, dns_cache_t *cache,
                                      dns_result_t *result, int debug, int depth,
                                      char *err, size_t err_len) {
    if (depth > DNS_MAX_DEPTH) {
        snprintf(err, err_len, "превышена глубина резолвинга");
        return -1;
    }

    char normalized[DNS_MAX_NAME];
    dns_normalize_name(qname, normalized, sizeof(normalized));

    size_t cache_count = 0;
    if (cache_get(cache, normalized, qtype, result->records, DNS_MAX_RECORDS, &cache_count) == 0) {
        result->count = cache_count;
        return 0;
    }

    dns_server_list_t servers;
    init_root_servers(&servers);
    if (servers.count == 0) {
        snprintf(err, err_len, "не заданы корневые серверы");
        return -1;
    }

    char current_name[DNS_MAX_NAME];
    strncpy(current_name, normalized, sizeof(current_name) - 1);
    current_name[sizeof(current_name) - 1] = '\0';

    for (int iteration = 0; iteration < DNS_MAX_ITERATIONS; iteration++) {
        int progressed = 0;
        for (size_t s = 0; s < servers.count; s++) {
            uint16_t id = (uint16_t)rand();
            uint8_t query[512];
            size_t query_len = dns_build_query(current_name, qtype, id, query, sizeof(query));
            if (query_len == 0) {
                snprintf(err, err_len, "не удалось сформировать запрос");
                return -1;
            }

            dns_message_t msg;
            if (query_server(&servers.servers[s], query, query_len, &msg, debug) != 0) {
                continue;
            }

            int rcode = dns_rcode(&msg);
            if (rcode == 3) {
                snprintf(err, err_len, "NXDOMAIN (домен не существует)");
                return -1;
            }
            if (rcode != 0) {
                snprintf(err, err_len, "ошибка DNS: rcode=%d", rcode);
                continue;
            }

            if (collect_answer_records(&msg, current_name, qtype, result) == 0) {
                cache_put(cache, current_name, qtype, result->records, result->count);
                return 0;
            }

            if (qtype != DNS_TYPE_CNAME) {
                const dns_record_t *cname = find_cname_record(&msg, current_name);
                if (cname) {
                    debug_log(debug, "Переход по CNAME %s -> %s", current_name, cname->data);
                    dns_result_t cname_result;
                    cname_result.count = 1;
                    cname_result.records[0] = *cname;
                    cache_put(cache, current_name, DNS_TYPE_CNAME, cname_result.records, cname_result.count);

                    strncpy(current_name, cname->data, sizeof(current_name) - 1);
                    current_name[sizeof(current_name) - 1] = '\0';
                    init_root_servers(&servers);
                    progressed = 1;
                    break;
                }
            }

            char ns_names[DNS_MAX_RECORDS][DNS_MAX_NAME];
            size_t ns_count = 0;
            if (collect_ns_names(&msg, ns_names, &ns_count) != 0) {
                continue;
            }

            dns_server_list_t next_servers;
            if (collect_glue_servers(&msg, ns_names, ns_count, &next_servers) == 0) {
                servers = next_servers;
                progressed = 1;
                break;
            }

            next_servers.count = 0;
            for (size_t i = 0; i < ns_count; i++) {
                if (resolve_ns_addresses(ns_names[i], cache, &next_servers, debug, depth) == 0) {
                    debug_log(debug, "NS %s разрешён в %zu адрес(ов)", ns_names[i], next_servers.count);
                }
            }
            if (next_servers.count > 0) {
                servers = next_servers;
                progressed = 1;
                break;
            }
        }

        if (!progressed) {
            snprintf(err, err_len, "нет подходящего реферала");
            return -1;
        }
    }

    snprintf(err, err_len, "превышен лимит итераций");
    return -1;
}

int resolve_iterative(const char *qname, uint16_t qtype, dns_cache_t *cache,
                      dns_result_t *result, int debug,
                      char *err, size_t err_len) {
    if (!qname || !cache || !result || !err) {
        return -1;
    }
    return resolve_iterative_internal(qname, qtype, cache, result, debug, 0, err, err_len);
}
