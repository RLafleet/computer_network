#ifndef RDT_COMMON_H
#define RDT_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>

#define RDT_MAX_DATA 1024
#define RDT_HDR_SIZE 16
#define RDT_MAX_PACKET (RDT_HDR_SIZE + RDT_MAX_DATA)

#define RDT_FLAG_ACK  0x01
#define RDT_FLAG_SYN  0x02
#define RDT_FLAG_FIN  0x04
#define RDT_FLAG_DATA 0x08

typedef struct {
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t len;
    uint32_t crc32;
} rdt_header_t;

typedef struct {
    int loss_pct;
    int delay_ms;
    int corrupt_pct;
} rdt_sim_t;

typedef struct {
    uint64_t sent_packets;
    uint64_t sent_bytes;
    uint64_t resent_packets;
    uint64_t dropped_sim;
    uint64_t corrupted_sim;
    uint64_t recv_packets;
    uint64_t bad_crc;
    uint64_t acks_sent;
    uint64_t acks_recv;
    uint64_t dup_acks;
} rdt_stats_t;

typedef struct {
    double srtt;
    double rttvar;
    double rto;
    int initialized;
} rdt_rtt_t;

uint32_t rdt_crc32(const uint8_t *data, size_t len);
uint64_t rdt_hton64(uint64_t value);
uint64_t rdt_ntoh64(uint64_t value);
size_t rdt_build_packet(uint8_t *buf, size_t buf_len, uint32_t seq, uint32_t ack,
                        uint16_t flags, const uint8_t *data, uint16_t len);
int rdt_parse_packet(const uint8_t *buf, size_t len, rdt_header_t *hdr,
                     uint8_t *payload, size_t payload_cap);
const char *rdt_flags_str(uint16_t flags);

void rdt_stats_init(rdt_stats_t *stats);
void rdt_rtt_init(rdt_rtt_t *rtt, double initial_ms);
void rdt_rtt_update(rdt_rtt_t *rtt, double sample_ms);

uint64_t rdt_now_ms(void);
uint64_t rdt_elapsed_ms(const struct timeval *since);

void rdt_log_info(const char *fmt, ...);
void rdt_log_debug(int enabled, const char *fmt, ...);

int rdt_sim_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest, socklen_t addrlen,
                   const rdt_sim_t *sim, rdt_stats_t *stats, int debug);

#endif
