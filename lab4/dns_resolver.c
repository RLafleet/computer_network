#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DNS_MAX_NAME 255
#define DNS_MAX_RDATA_STR 512
#define DNS_MAX_RECORDS 64

#define DNS_CLASS_IN 1

#define DNS_TYPE_A 1
#define DNS_TYPE_NS 2
#define DNS_TYPE_CNAME 5
#define DNS_TYPE_SOA 6
#define DNS_TYPE_PTR 12
#define DNS_TYPE_MX 15
#define DNS_TYPE_TXT 16
#define DNS_TYPE_AAAA 28
#define DNS_TYPE_ANY 255

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

typedef struct {
    char name[DNS_MAX_NAME];
    uint16_t type;
    uint16_t class_code;
} dns_question_t;

typedef struct {
    char name[DNS_MAX_NAME];
    uint16_t type;
    uint16_t class_code;
    uint32_t ttl;
    char data[DNS_MAX_RDATA_STR];
    uint16_t preference;
    int has_preference;
} dns_record_t;

typedef struct {
    dns_header_t header;
    dns_question_t question;
    dns_record_t answers[DNS_MAX_RECORDS];
    size_t answer_count;
    dns_record_t authorities[DNS_MAX_RECORDS];
    size_t authority_count;
    dns_record_t additionals[DNS_MAX_RECORDS];
    size_t additional_count;
} dns_message_t;

const char *dns_type_to_string(uint16_t type);
int dns_type_from_string(const char *str, uint16_t *out_type);
size_t dns_build_query(const char *qname, uint16_t qtype, uint16_t id,
                       uint8_t *out, size_t out_len);
int dns_parse_response(const uint8_t *buf, size_t len, dns_message_t *out_msg);

int dns_is_tc(const dns_message_t *msg);
int dns_rcode(const dns_message_t *msg);

void dns_normalize_name(const char *input, char *output, size_t out_len);

int send_all(int fd, const uint8_t *buf, size_t len);
int recv_all(int fd, uint8_t *buf, size_t len);

int sockaddr_to_string(const struct sockaddr *addr, char *out, size_t out_len);
int make_sockaddr(const char *ip, uint16_t port,
                  struct sockaddr_storage *out, socklen_t *out_len);

int dns_udp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms);

int dns_tcp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms);

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

typedef struct {
    dns_record_t records[DNS_MAX_RECORDS];
    size_t count;
} dns_result_t;

int resolve_iterative(const char *qname, uint16_t qtype, dns_cache_t *cache,
                      dns_result_t *result, int debug,
                      char *err, size_t err_len);

#define DNS_HEADER_SIZE 12

static void bytes_to_hex(const uint8_t *data, size_t len, char *out, size_t out_len) {
    static const char hex[] = "0123456789abcdef";
    size_t pos = 0;
    if (out_len == 0) {
        return;
    }
    if (out_len >= 2) {
        out[pos++] = '0';
        out[pos++] = 'x';
    }
    for (size_t i = 0; i < len && pos + 2 < out_len; i++) {
        out[pos++] = hex[(data[i] >> 4) & 0x0f];
        out[pos++] = hex[data[i] & 0x0f];
    }
    if (pos < out_len) {
        out[pos] = '\0';
    } else {
        out[out_len - 1] = '\0';
    }
}

void dns_normalize_name(const char *input, char *output, size_t out_len) {
    size_t len = strlen(input);
    while (len > 0 && input[len - 1] == '.') {
        len--;
    }
    size_t out_pos = 0;
    for (size_t i = 0; i < len && out_pos + 1 < out_len; i++) {
        unsigned char ch = (unsigned char)input[i];
        output[out_pos++] = (char)tolower(ch);
    }
    output[out_pos] = '\0';
}

const char *dns_type_to_string(uint16_t type) {
    switch (type) {
        case DNS_TYPE_A:
            return "A";
        case DNS_TYPE_NS:
            return "NS";
        case DNS_TYPE_CNAME:
            return "CNAME";
        case DNS_TYPE_SOA:
            return "SOA";
        case DNS_TYPE_PTR:
            return "PTR";
        case DNS_TYPE_MX:
            return "MX";
        case DNS_TYPE_TXT:
            return "TXT";
        case DNS_TYPE_AAAA:
            return "AAAA";
        case DNS_TYPE_ANY:
            return "ANY";
        default:
            return "UNKNOWN";
    }
}

int dns_type_from_string(const char *str, uint16_t *out_type) {
    if (str == NULL || out_type == NULL) {
        return -1;
    }
    if (strcasecmp(str, "A") == 0) {
        *out_type = DNS_TYPE_A;
    } else if (strcasecmp(str, "AAAA") == 0) {
        *out_type = DNS_TYPE_AAAA;
    } else if (strcasecmp(str, "NS") == 0) {
        *out_type = DNS_TYPE_NS;
    } else if (strcasecmp(str, "MX") == 0) {
        *out_type = DNS_TYPE_MX;
    } else if (strcasecmp(str, "CNAME") == 0) {
        *out_type = DNS_TYPE_CNAME;
    } else if (strcasecmp(str, "PTR") == 0) {
        *out_type = DNS_TYPE_PTR;
    } else if (strcasecmp(str, "SOA") == 0) {
        *out_type = DNS_TYPE_SOA;
    } else if (strcasecmp(str, "TXT") == 0) {
        *out_type = DNS_TYPE_TXT;
    } else if (strcasecmp(str, "ANY") == 0) {
        *out_type = DNS_TYPE_ANY;
    } else {
        return -1;
    }
    return 0;
}

static int dns_write_name(const char *name, uint8_t *out, size_t out_len) {
    if (out_len == 0) {
        return -1;
    }
    if (name[0] == '\0') {
        if (out_len < 1) {
            return -1;
        }
        out[0] = 0;
        return 1;
    }

    size_t pos = 0;
    const char *label = name;
    while (*label != '\0') {
        const char *dot = strchr(label, '.');
        size_t label_len = dot ? (size_t)(dot - label) : strlen(label);
        if (label_len > 63 || pos + 1 + label_len >= out_len) {
            return -1;
        }
        out[pos++] = (uint8_t)label_len;
        memcpy(out + pos, label, label_len);
        pos += label_len;
        if (!dot) {
            break;
        }
        label = dot + 1;
    }

    if (pos >= out_len) {
        return -1;
    }
    out[pos++] = 0;
    return (int)pos;
}

static int dns_read_name(const uint8_t *buf, size_t len, size_t *offset,
                         char *out, size_t out_len) {
    size_t pos = *offset;
    size_t out_pos = 0;
    int jumped = 0;
    size_t jump_pos = 0;
    int depth = 0;

    while (pos < len) {
        uint8_t label = buf[pos];
        if (label == 0) {
            pos++;
            break;
        }
        if ((label & 0xC0) == 0xC0) {
            if (pos + 1 >= len) {
                return -1;
            }
            uint16_t ptr = (uint16_t)(((label & 0x3F) << 8) | buf[pos + 1]);
            if (!jumped) {
                jump_pos = pos + 2;
            }
            pos = ptr;
            jumped = 1;
            if (++depth > 10) {
                return -1;
            }
            continue;
        }
        pos++;
        if (pos + label > len) {
            return -1;
        }
        if (out_pos != 0) {
            if (out_pos + 1 >= out_len) {
                return -1;
            }
            out[out_pos++] = '.';
        }
        if (out_pos + label >= out_len) {
            return -1;
        }
        memcpy(out + out_pos, buf + pos, label);
        out_pos += label;
        pos += label;
    }

    if (out_pos == 0) {
        if (out_len < 2) {
            return -1;
        }
        out[0] = '.';
        out[1] = '\0';
    } else {
        out[out_pos] = '\0';
    }

    if (!jumped) {
        *offset = pos;
    } else {
        *offset = jump_pos;
    }
    dns_normalize_name(out, out, out_len);
    return 0;
}

size_t dns_build_query(const char *qname, uint16_t qtype, uint16_t id,
                       uint8_t *out, size_t out_len) {
    if (out_len < DNS_HEADER_SIZE) {
        return 0;
    }
    memset(out, 0, out_len);

    uint16_t flags = htons(0);
    uint16_t qdcount = htons(1);
    uint16_t net_id = htons(id);

    memcpy(out, &net_id, sizeof(net_id));
    memcpy(out + 2, &flags, sizeof(flags));
    memcpy(out + 4, &qdcount, sizeof(qdcount));

    size_t offset = DNS_HEADER_SIZE;
    char normalized[DNS_MAX_NAME];
    dns_normalize_name(qname, normalized, sizeof(normalized));

    int name_len = dns_write_name(normalized, out + offset, out_len - offset);
    if (name_len < 0) {
        return 0;
    }
    offset += (size_t)name_len;

    if (offset + 4 > out_len) {
        return 0;
    }
    uint16_t net_type = htons(qtype);
    uint16_t net_class = htons(DNS_CLASS_IN);
    memcpy(out + offset, &net_type, sizeof(net_type));
    offset += sizeof(net_type);
    memcpy(out + offset, &net_class, sizeof(net_class));
    offset += sizeof(net_class);

    return offset;
}

static int parse_rdata(const uint8_t *buf, size_t len, size_t rdata_offset,
                       uint16_t type, uint16_t rdlength, dns_record_t *rec) {
    if (rdata_offset + rdlength > len) {
        return -1;
    }
    rec->has_preference = 0;
    rec->preference = 0;
    rec->data[0] = '\0';

    if (type == DNS_TYPE_A && rdlength == 4) {
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, buf + rdata_offset, ip, sizeof(ip)) == NULL) {
            return -1;
        }
        strncpy(rec->data, ip, sizeof(rec->data) - 1);
        rec->data[sizeof(rec->data) - 1] = '\0';
        return 0;
    }
    if (type == DNS_TYPE_AAAA && rdlength == 16) {
        char ip[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, buf + rdata_offset, ip, sizeof(ip)) == NULL) {
            return -1;
        }
        strncpy(rec->data, ip, sizeof(rec->data) - 1);
        rec->data[sizeof(rec->data) - 1] = '\0';
        return 0;
    }
    if (type == DNS_TYPE_NS || type == DNS_TYPE_CNAME || type == DNS_TYPE_PTR) {
        size_t name_offset = rdata_offset;
        if (dns_read_name(buf, len, &name_offset, rec->data, sizeof(rec->data)) != 0) {
            return -1;
        }
        return 0;
    }
    if (type == DNS_TYPE_MX) {
        if (rdlength < 2) {
            return -1;
        }
        uint16_t pref = 0;
        memcpy(&pref, buf + rdata_offset, sizeof(pref));
        pref = ntohs(pref);
        size_t name_offset = rdata_offset + 2;
        if (dns_read_name(buf, len, &name_offset, rec->data, sizeof(rec->data)) != 0) {
            return -1;
        }
        rec->has_preference = 1;
        rec->preference = pref;
        return 0;
    }
    if (type == DNS_TYPE_TXT) {
        if (rdlength < 1) {
            return -1;
        }
        uint8_t txt_len = buf[rdata_offset];
        size_t copy_len = txt_len;
        if (copy_len + 1 > rdlength) {
            copy_len = rdlength > 0 ? rdlength - 1 : 0;
        }
        if (copy_len >= sizeof(rec->data)) {
            copy_len = sizeof(rec->data) - 1;
        }
        memcpy(rec->data, buf + rdata_offset + 1, copy_len);
        rec->data[copy_len] = '\0';
        return 0;
    }

    bytes_to_hex(buf + rdata_offset, rdlength, rec->data, sizeof(rec->data));
    return 0;
}

int dns_parse_response(const uint8_t *buf, size_t len, dns_message_t *out_msg) {
    if (len < DNS_HEADER_SIZE || out_msg == NULL) {
        return -1;
    }
    memset(out_msg, 0, sizeof(*out_msg));

    dns_header_t header;
    memcpy(&header.id, buf, 2);
    memcpy(&header.flags, buf + 2, 2);
    memcpy(&header.qdcount, buf + 4, 2);
    memcpy(&header.ancount, buf + 6, 2);
    memcpy(&header.nscount, buf + 8, 2);
    memcpy(&header.arcount, buf + 10, 2);

    header.id = ntohs(header.id);
    header.flags = ntohs(header.flags);
    header.qdcount = ntohs(header.qdcount);
    header.ancount = ntohs(header.ancount);
    header.nscount = ntohs(header.nscount);
    header.arcount = ntohs(header.arcount);

    out_msg->header = header;

    size_t offset = DNS_HEADER_SIZE;

    if (header.qdcount > 0) {
        if (dns_read_name(buf, len, &offset, out_msg->question.name,
                          sizeof(out_msg->question.name)) != 0) {
            return -1;
        }
        if (offset + 4 > len) {
            return -1;
        }
        uint16_t qtype = 0;
        uint16_t qclass = 0;
        memcpy(&qtype, buf + offset, 2);
        offset += 2;
        memcpy(&qclass, buf + offset, 2);
        offset += 2;
        out_msg->question.type = ntohs(qtype);
        out_msg->question.class_code = ntohs(qclass);
    }

    for (uint16_t i = 0; i < header.ancount && out_msg->answer_count < DNS_MAX_RECORDS; i++) {
        dns_record_t *rec = &out_msg->answers[out_msg->answer_count];
        if (dns_read_name(buf, len, &offset, rec->name, sizeof(rec->name)) != 0) {
            return -1;
        }
        if (offset + 10 > len) {
            return -1;
        }
        uint16_t type = 0;
        uint16_t class_code = 0;
        uint32_t ttl = 0;
        uint16_t rdlength = 0;
        memcpy(&type, buf + offset, 2);
        offset += 2;
        memcpy(&class_code, buf + offset, 2);
        offset += 2;
        memcpy(&ttl, buf + offset, 4);
        offset += 4;
        memcpy(&rdlength, buf + offset, 2);
        offset += 2;

        rec->type = ntohs(type);
        rec->class_code = ntohs(class_code);
        rec->ttl = ntohl(ttl);
        rdlength = ntohs(rdlength);
        if (parse_rdata(buf, len, offset, rec->type, rdlength, rec) != 0) {
            return -1;
        }
        offset += rdlength;
        out_msg->answer_count++;
    }

    for (uint16_t i = 0; i < header.nscount && out_msg->authority_count < DNS_MAX_RECORDS; i++) {
        dns_record_t *rec = &out_msg->authorities[out_msg->authority_count];
        if (dns_read_name(buf, len, &offset, rec->name, sizeof(rec->name)) != 0) {
            return -1;
        }
        if (offset + 10 > len) {
            return -1;
        }
        uint16_t type = 0;
        uint16_t class_code = 0;
        uint32_t ttl = 0;
        uint16_t rdlength = 0;
        memcpy(&type, buf + offset, 2);
        offset += 2;
        memcpy(&class_code, buf + offset, 2);
        offset += 2;
        memcpy(&ttl, buf + offset, 4);
        offset += 4;
        memcpy(&rdlength, buf + offset, 2);
        offset += 2;

        rec->type = ntohs(type);
        rec->class_code = ntohs(class_code);
        rec->ttl = ntohl(ttl);
        rdlength = ntohs(rdlength);
        if (parse_rdata(buf, len, offset, rec->type, rdlength, rec) != 0) {
            return -1;
        }
        offset += rdlength;
        out_msg->authority_count++;
    }

    for (uint16_t i = 0; i < header.arcount && out_msg->additional_count < DNS_MAX_RECORDS; i++) {
        dns_record_t *rec = &out_msg->additionals[out_msg->additional_count];
        if (dns_read_name(buf, len, &offset, rec->name, sizeof(rec->name)) != 0) {
            return -1;
        }
        if (offset + 10 > len) {
            return -1;
        }
        uint16_t type = 0;
        uint16_t class_code = 0;
        uint32_t ttl = 0;
        uint16_t rdlength = 0;
        memcpy(&type, buf + offset, 2);
        offset += 2;
        memcpy(&class_code, buf + offset, 2);
        offset += 2;
        memcpy(&ttl, buf + offset, 4);
        offset += 4;
        memcpy(&rdlength, buf + offset, 2);
        offset += 2;

        rec->type = ntohs(type);
        rec->class_code = ntohs(class_code);
        rec->ttl = ntohl(ttl);
        rdlength = ntohs(rdlength);
        if (parse_rdata(buf, len, offset, rec->type, rdlength, rec) != 0) {
            return -1;
        }
        offset += rdlength;
        out_msg->additional_count++;
    }

    return 0;
}

int dns_is_tc(const dns_message_t *msg) {
    if (msg == NULL) {
        return 0;
    }
    return (msg->header.flags & 0x0200) != 0;
}

int dns_rcode(const dns_message_t *msg) {
    if (msg == NULL) {
        return -1;
    }
    return (int)(msg->header.flags & 0x000F);
}

int send_all(int fd, const uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(fd, buf + total, len - total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        total += (size_t)sent;
    }
    return 0;
}

int recv_all(int fd, uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t recvd = recv(fd, buf + total, len - total, 0);
        if (recvd < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (recvd == 0) {
            return -1;
        }
        total += (size_t)recvd;
    }
    return 0;
}

int sockaddr_to_string(const struct sockaddr *addr, char *out, size_t out_len) {
    if (addr == NULL || out == NULL || out_len == 0) {
        return -1;
    }
    void *src = NULL;
    uint16_t port = 0;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        src = (void *)&in->sin_addr;
        port = ntohs(in->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)addr;
        src = (void *)&in6->sin6_addr;
        port = ntohs(in6->sin6_port);
    } else {
        return -1;
    }

    char ip[INET6_ADDRSTRLEN];
    if (inet_ntop(addr->sa_family, src, ip, sizeof(ip)) == NULL) {
        return -1;
    }
    if (snprintf(out, out_len, "%s:%u", ip, port) >= (int)out_len) {
        return -1;
    }
    return 0;
}

int make_sockaddr(const char *ip, uint16_t port,
                  struct sockaddr_storage *out, socklen_t *out_len) {
    if (ip == NULL || out == NULL || out_len == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    struct sockaddr_in in;
    if (inet_pton(AF_INET, ip, &in.sin_addr) == 1) {
        in.sin_family = AF_INET;
        in.sin_port = htons(port);
        memcpy(out, &in, sizeof(in));
        *out_len = sizeof(in);
        return 0;
    }

    struct sockaddr_in6 in6;
    if (inet_pton(AF_INET6, ip, &in6.sin6_addr) == 1) {
        in6.sin6_family = AF_INET6;
        in6.sin6_port = htons(port);
        memcpy(out, &in6, sizeof(in6));
        *out_len = sizeof(in6);
        return 0;
    }
    return -1;
}

int dns_udp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms) {
    if (!addr || !query || !response || !response_len) {
        return -1;
    }

    int sock = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, addr, addrlen) != 0) {
        close(sock);
        return -1;
    }

    ssize_t sent = send(sock, query, query_len, 0);
    if (sent < 0 || (size_t)sent != query_len) {
        close(sock);
        return -1;
    }

    ssize_t recvd = recv(sock, response, *response_len, 0);
    if (recvd < 0) {
        close(sock);
        return -1;
    }
    *response_len = (size_t)recvd;
    close(sock);
    return 0;
}

int dns_tcp_query(const struct sockaddr *addr, socklen_t addrlen,
                  const uint8_t *query, size_t query_len,
                  uint8_t *response, size_t *response_len,
                  int timeout_ms) {
    if (!addr || !query || !response || !response_len) {
        return -1;
    }

    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, addr, addrlen) != 0) {
        close(sock);
        return -1;
    }

    uint16_t net_len = htons((uint16_t)query_len);
    if (send_all(sock, (const uint8_t *)&net_len, sizeof(net_len)) != 0) {
        close(sock);
        return -1;
    }
    if (send_all(sock, query, query_len) != 0) {
        close(sock);
        return -1;
    }

    uint16_t resp_len = 0;
    if (recv_all(sock, (uint8_t *)&resp_len, sizeof(resp_len)) != 0) {
        close(sock);
        return -1;
    }
    resp_len = ntohs(resp_len);
    if (resp_len == 0 || resp_len > *response_len) {
        close(sock);
        return -1;
    }

    if (recv_all(sock, response, resp_len) != 0) {
        close(sock);
        return -1;
    }
    *response_len = resp_len;

    close(sock);
    return 0;
}

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
