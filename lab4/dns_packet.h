#ifndef DNS_PACKET_H
#define DNS_PACKET_H

#include <stddef.h>
#include <stdint.h>

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

#endif
