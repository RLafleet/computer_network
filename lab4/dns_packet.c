#include "dns_packet.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

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
