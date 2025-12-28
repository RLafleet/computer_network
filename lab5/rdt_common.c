#include "rdt_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t crc_table[256];
static int crc_table_ready = 0;

static void rdt_crc32_init(void) {
    /* Предрасчет таблицы CRC32 */
    if (crc_table_ready) {
        return;
    }
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1) {
                c = 0xEDB88320U ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }
        crc_table[i] = c;
    }
    crc_table_ready = 1;
}

uint32_t rdt_crc32(const uint8_t *data, size_t len) {
    rdt_crc32_init();
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; ++i) {
        uint8_t idx = (uint8_t)((crc ^ data[i]) & 0xFF);
        crc = crc_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

uint64_t rdt_hton64(uint64_t value) {
    uint32_t low = htonl((uint32_t)(value & 0xFFFFFFFFULL));
    uint32_t high = htonl((uint32_t)(value >> 32));
    return ((uint64_t)low << 32) | high;
}

uint64_t rdt_ntoh64(uint64_t value) {
    uint32_t low = ntohl((uint32_t)(value & 0xFFFFFFFFULL));
    uint32_t high = ntohl((uint32_t)(value >> 32));
    return ((uint64_t)low << 32) | high;
}

size_t rdt_build_packet(uint8_t *buf, size_t buf_len, uint32_t seq, uint32_t ack,
                        uint16_t flags, const uint8_t *data, uint16_t len) {
    if (buf_len < (size_t)RDT_HDR_SIZE + len) {
        return 0;
    }

    uint32_t seq_n = htonl(seq);
    uint32_t ack_n = htonl(ack);
    uint16_t flags_n = htons(flags);
    uint16_t len_n = htons(len);
    uint32_t crc_n = 0;

    memcpy(buf, &seq_n, sizeof(seq_n));
    memcpy(buf + 4, &ack_n, sizeof(ack_n));
    memcpy(buf + 8, &flags_n, sizeof(flags_n));
    memcpy(buf + 10, &len_n, sizeof(len_n));
    memcpy(buf + 12, &crc_n, sizeof(crc_n));

    if (len > 0 && data != NULL) {
        memcpy(buf + RDT_HDR_SIZE, data, len);
    }

    uint32_t crc = rdt_crc32(buf, RDT_HDR_SIZE + len);
    crc_n = htonl(crc);
    memcpy(buf + 12, &crc_n, sizeof(crc_n));

    return RDT_HDR_SIZE + len;
}

int rdt_parse_packet(const uint8_t *buf, size_t len, rdt_header_t *hdr,
                     uint8_t *payload, size_t payload_cap) {
    if (len < RDT_HDR_SIZE || hdr == NULL) {
        return -1;
    }

    uint32_t seq_n = 0;
    uint32_t ack_n = 0;
    uint16_t flags_n = 0;
    uint16_t len_n = 0;
    uint32_t crc_n = 0;

    memcpy(&seq_n, buf, sizeof(seq_n));
    memcpy(&ack_n, buf + 4, sizeof(ack_n));
    memcpy(&flags_n, buf + 8, sizeof(flags_n));
    memcpy(&len_n, buf + 10, sizeof(len_n));
    memcpy(&crc_n, buf + 12, sizeof(crc_n));

    hdr->seq = ntohl(seq_n);
    hdr->ack = ntohl(ack_n);
    hdr->flags = ntohs(flags_n);
    hdr->len = ntohs(len_n);
    hdr->crc32 = ntohl(crc_n);

    if (hdr->len > RDT_MAX_DATA || len < (size_t)RDT_HDR_SIZE + hdr->len) {
        return -1;
    }
    if (payload && hdr->len > payload_cap) {
        return -1;
    }

    uint8_t temp[RDT_MAX_PACKET];
    if ((size_t)RDT_HDR_SIZE + hdr->len > sizeof(temp)) {
        return -1;
    }
    memcpy(temp, buf, RDT_HDR_SIZE + hdr->len);
    uint32_t zero = 0;
    memcpy(temp + 12, &zero, sizeof(zero));
    uint32_t crc_calc = rdt_crc32(temp, RDT_HDR_SIZE + hdr->len);

    if (crc_calc != hdr->crc32) {
        return -2;
    }

    if (payload && hdr->len > 0) {
        memcpy(payload, buf + RDT_HDR_SIZE, hdr->len);
    }

    return 0;
}

const char *rdt_flags_str(uint16_t flags) {
    static char buf[32];
    buf[0] = '\0';
    if (flags & RDT_FLAG_ACK) {
        strncat(buf, "ACK|", sizeof(buf) - strlen(buf) - 1);
    }
    if (flags & RDT_FLAG_SYN) {
        strncat(buf, "SYN|", sizeof(buf) - strlen(buf) - 1);
    }
    if (flags & RDT_FLAG_FIN) {
        strncat(buf, "FIN|", sizeof(buf) - strlen(buf) - 1);
    }
    if (flags & RDT_FLAG_DATA) {
        strncat(buf, "DATA|", sizeof(buf) - strlen(buf) - 1);
    }
    size_t n = strlen(buf);
    if (n == 0) {
        snprintf(buf, sizeof(buf), "NONE");
    } else {
        buf[n - 1] = '\0';
    }
    return buf;
}

void rdt_stats_init(rdt_stats_t *stats) {
    if (!stats) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
}

void rdt_rtt_init(rdt_rtt_t *rtt, double initial_ms) {
    if (!rtt) {
        return;
    }
    rtt->srtt = initial_ms;
    rtt->rttvar = initial_ms / 2.0;
    rtt->rto = rtt->srtt + 4.0 * rtt->rttvar;
    rtt->initialized = 1;
}

void rdt_rtt_update(rdt_rtt_t *rtt, double sample_ms) {
    if (!rtt) {
        return;
    }
    const double alpha = 0.125;
    const double beta = 0.25;

    /* Классическая формула TCP для адаптивного таймаута */
    if (!rtt->initialized) {
        rdt_rtt_init(rtt, sample_ms);
        return;
    }

    double err = sample_ms - rtt->srtt;
    rtt->srtt += alpha * err;
    rtt->rttvar += beta * (fabs(err) - rtt->rttvar);
    rtt->rto = rtt->srtt + 4.0 * rtt->rttvar;

    if (rtt->rto < 100.0) {
        rtt->rto = 100.0;
    } else if (rtt->rto > 2000.0) {
        rtt->rto = 2000.0;
    }
}

uint64_t rdt_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

uint64_t rdt_elapsed_ms(const struct timeval *since) {
    if (!since) {
        return 0;
    }
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t start_ms = (uint64_t)since->tv_sec * 1000ULL + (uint64_t)(since->tv_usec / 1000);
    uint64_t now_ms = (uint64_t)now.tv_sec * 1000ULL + (uint64_t)(now.tv_usec / 1000);
    return now_ms - start_ms;
}

void rdt_log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void rdt_log_debug(int enabled, const char *fmt, ...) {
    if (!enabled) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static int rdt_rand_pct(void) {
    return rand() % 100;
}

static void rdt_sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int rdt_sim_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest, socklen_t addrlen,
                   const rdt_sim_t *sim, rdt_stats_t *stats, int debug) {
    if (!buf || len == 0) {
        return -1;
    }

    if (sim && sim->loss_pct > 0 && rdt_rand_pct() < sim->loss_pct) {
        if (stats) {
            stats->dropped_sim++;
        }
        rdt_log_debug(debug, "[симуляция] потеря пакета");
        return 0;
    }

    if (sim && sim->delay_ms > 0) {
        int delay = rand() % (sim->delay_ms + 1);
        rdt_sleep_ms(delay);
    }

    const void *send_buf = buf;
    uint8_t temp[RDT_MAX_PACKET];

    if (sim && sim->corrupt_pct > 0 && rdt_rand_pct() < sim->corrupt_pct) {
        if (len > sizeof(temp)) {
            return -1;
        }
        memcpy(temp, buf, len);
        size_t idx = (size_t)(rand() % len);
        temp[idx] ^= 0xFF;
        send_buf = temp;
        if (stats) {
            stats->corrupted_sim++;
        }
        rdt_log_debug(debug, "[симуляция] повреждение пакета");
    }

    ssize_t sent = sendto(sockfd, send_buf, len, flags, dest, addrlen);
    if (sent < 0) {
        return -1;
    }

    if (stats) {
        stats->sent_packets++;
        stats->sent_bytes += (uint64_t)sent;
    }

    return (int)sent;
}
