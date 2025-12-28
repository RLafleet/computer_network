#include "rdt_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

typedef enum {
    ALG_GBN = 0,
    ALG_SR = 1
} rdt_alg_t;

typedef struct {
    rdt_alg_t alg;
    int debug;
    int loss_pct;
    int delay_ms;
    int corrupt_pct;
    int window;
    const char *port;
    const char *out_path;
} recv_opts_t;

typedef struct {
    uint32_t seq;
    uint16_t len;
    uint16_t flags;
    uint8_t data[RDT_MAX_DATA];
    int received;
} recv_slot_t;

static void receiver_usage(const char *prog) {
    fprintf(stderr,
            "Использование: %s -alg GBN|SR [-d] [-loss pct] [-delay ms] [-corrupt pct] [-w окно] port output\n",
            prog);
}

static int parse_alg(const char *s, rdt_alg_t *alg) {
    if (strcmp(s, "GBN") == 0) {
        *alg = ALG_GBN;
        return 0;
    }
    if (strcmp(s, "SR") == 0) {
        *alg = ALG_SR;
        return 0;
    }
    return -1;
}

static int parse_receiver_args(int argc, char **argv, recv_opts_t *opts) {
    if (!opts) {
        return -1;
    }
    memset(opts, 0, sizeof(*opts));
    opts->window = 5;

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-alg") == 0 && i + 1 < argc) {
            if (parse_alg(argv[i + 1], &opts->alg) != 0) {
                return -1;
            }
            i += 2;
        } else if (strcmp(argv[i], "-d") == 0) {
            opts->debug = 1;
            i += 1;
        } else if (strcmp(argv[i], "-loss") == 0 && i + 1 < argc) {
            opts->loss_pct = atoi(argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-delay") == 0 && i + 1 < argc) {
            opts->delay_ms = atoi(argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-corrupt") == 0 && i + 1 < argc) {
            opts->corrupt_pct = atoi(argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            opts->window = atoi(argv[i + 1]);
            i += 2;
        } else {
            break;
        }
    }

    if (i + 2 != argc) {
        return -1;
    }
    opts->port = argv[i];
    opts->out_path = argv[i + 1];

    if (opts->window <= 0) {
        opts->window = 1;
    }
    return 0;
}

static int send_ack(int sockfd, const struct sockaddr *addr, socklen_t addrlen,
                    uint32_t ackno, const rdt_sim_t *sim, rdt_stats_t *stats, int debug) {
    uint8_t buf[RDT_MAX_PACKET];
    size_t pkt_len = rdt_build_packet(buf, sizeof(buf), 0, ackno, RDT_FLAG_ACK, NULL, 0);
    if (pkt_len == 0) {
        return -1;
    }
    int rc = rdt_sim_sendto(sockfd, buf, pkt_len, 0, addr, addrlen, sim, stats, debug);
    if (rc >= 0) {
        if (stats) {
            stats->acks_sent++;
        }
        rdt_log_debug(debug, "[ACK] отправлен ack=%u", ackno);
    }
    return rc < 0 ? -1 : 0;
}

static void update_progress(uint64_t now_ms, uint64_t start_ms, uint64_t last_report,
                            uint64_t received_bytes, uint64_t total_bytes,
                            const rdt_stats_t *stats) {
    if (total_bytes == 0) {
        return;
    }
    if (now_ms - last_report < 1000) {
        return;
    }
    double percent = (received_bytes * 100.0) / (double)total_bytes;
    double elapsed = (now_ms - start_ms) / 1000.0;
    double speed = elapsed > 0 ? (received_bytes / 1024.0) / elapsed : 0.0;
    fprintf(stderr,
            "Прогресс: %.1f%%, скорость: %.2f КБ/с, сим.потери: %llu\n",
            percent, speed, (unsigned long long)(stats ? stats->dropped_sim : 0));
}

int main(int argc, char **argv) {
    recv_opts_t opts;
    if (parse_receiver_args(argc, argv, &opts) != 0) {
        receiver_usage(argv[0]);
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    FILE *out = fopen(opts.out_path, "wb");
    if (!out) {
        perror("Не удалось открыть файл для записи");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        fclose(out);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)atoi(opts.port));

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        fclose(out);
        return 1;
    }

    rdt_sim_t sim = {opts.loss_pct, opts.delay_ms, opts.corrupt_pct};
    rdt_stats_t stats;
    rdt_stats_init(&stats);

    uint32_t expected = 0;
    uint32_t base = 0;
    uint64_t total_size = 0;
    uint64_t received_bytes = 0;
    int finished = 0;
    uint64_t finish_time_ms = 0;
    const uint64_t finish_grace_ms = 2000;

    recv_slot_t *slots = NULL;
    /* Буфер приемника для SR: хранение пакетов вне порядка */
    if (opts.alg == ALG_SR) {
        slots = (recv_slot_t *)calloc(opts.window, sizeof(recv_slot_t));
        if (!slots) {
            rdt_log_info("Ошибка: недостаточно памяти для буфера SR");
            close(sockfd);
            fclose(out);
            return 1;
        }
    }

    uint64_t start_ms = rdt_now_ms();
    uint64_t last_report = start_ms;
    uint64_t last_recv_ms = start_ms;

    while (!finished || (finish_time_ms != 0 && rdt_now_ms() - finish_time_ms < finish_grace_ms)) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;

        int sel = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (sel < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        uint64_t now_ms = rdt_now_ms();
        update_progress(now_ms, start_ms, last_report, received_bytes, total_size, &stats);
        if (now_ms - last_report >= 1000) {
            last_report = now_ms;
        }
        if (!finished && now_ms - last_recv_ms > 30000) {
            rdt_log_info("Соединение потеряно (нет данных 30 секунд)");
            break;
        }

        if (sel <= 0) {
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            uint8_t buf[RDT_MAX_PACKET];
            struct sockaddr_storage from;
            socklen_t from_len = sizeof(from);
            ssize_t rcv = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len);
            if (rcv <= 0) {
                continue;
            }
            last_recv_ms = rdt_now_ms();
            stats.recv_packets++;

            rdt_header_t hdr;
            uint8_t payload[RDT_MAX_DATA];
            int parse_rc = rdt_parse_packet(buf, (size_t)rcv, &hdr, payload, sizeof(payload));
            if (parse_rc == -2) {
                stats.bad_crc++;
                rdt_log_debug(opts.debug, "[recv] поврежденный пакет seq=%u", hdr.seq);
                if (opts.alg == ALG_GBN) {
                    send_ack(sockfd, (struct sockaddr *)&from, from_len, expected, &sim, &stats, opts.debug);
                }
                continue;
            }
            if (parse_rc != 0) {
                continue;
            }

            if (hdr.flags & RDT_FLAG_ACK) {
                continue;
            }

            if (opts.alg == ALG_GBN) {
                if (hdr.seq == expected) {
                    if (hdr.flags & RDT_FLAG_SYN) {
                        if (hdr.len == 8) {
                            uint64_t net_size = 0;
                            memcpy(&net_size, payload, sizeof(net_size));
                            total_size = rdt_ntoh64(net_size);
                            rdt_log_debug(opts.debug, "[SYN] размер файла=%llu", (unsigned long long)total_size);
                        }
                    } else if (hdr.flags & RDT_FLAG_DATA) {
                        fwrite(payload, 1, hdr.len, out);
                        received_bytes += hdr.len;
                    } else if (hdr.flags & RDT_FLAG_FIN) {
                        finished = 1;
                        if (finish_time_ms == 0) {
                            finish_time_ms = rdt_now_ms();
                        }
                    }
                    expected++;
                    send_ack(sockfd, (struct sockaddr *)&from, from_len, expected, &sim, &stats, opts.debug);
                } else {
                    send_ack(sockfd, (struct sockaddr *)&from, from_len, expected, &sim, &stats, opts.debug);
                    rdt_log_debug(opts.debug, "[GBN] неожиданный seq=%u, ожидается=%u",
                                  hdr.seq, expected);
                }
            } else {
                if (hdr.seq >= base && hdr.seq < base + (uint32_t)opts.window) {
                    uint32_t idx = hdr.seq % (uint32_t)opts.window;
                    if (!slots[idx].received || slots[idx].seq != hdr.seq) {
                        slots[idx].seq = hdr.seq;
                        slots[idx].len = hdr.len;
                        slots[idx].flags = hdr.flags;
                        if (hdr.len > 0) {
                            memcpy(slots[idx].data, payload, hdr.len);
                        }
                        slots[idx].received = 1;
                        rdt_log_debug(opts.debug, "[SR] принят seq=%u", hdr.seq);
                    } else {
                        rdt_log_debug(opts.debug, "[SR] дубликат seq=%u", hdr.seq);
                    }
                    send_ack(sockfd, (struct sockaddr *)&from, from_len, hdr.seq, &sim, &stats, opts.debug);
                } else if (hdr.seq < base) {
                    send_ack(sockfd, (struct sockaddr *)&from, from_len, hdr.seq, &sim, &stats, opts.debug);
                } else {
                    rdt_log_debug(opts.debug, "[SR] пакет вне окна seq=%u", hdr.seq);
                }

                while (base < 0xFFFFFFFFU) {
                    uint32_t idx = base % (uint32_t)opts.window;
                    if (!slots[idx].received || slots[idx].seq != base) {
                        break;
                    }
                    if (slots[idx].flags & RDT_FLAG_SYN) {
                        if (slots[idx].len == 8) {
                            uint64_t net_size = 0;
                            memcpy(&net_size, slots[idx].data, sizeof(net_size));
                            total_size = rdt_ntoh64(net_size);
                            rdt_log_debug(opts.debug, "[SYN] размер файла=%llu", (unsigned long long)total_size);
                        }
                    } else if (slots[idx].flags & RDT_FLAG_DATA) {
                        fwrite(slots[idx].data, 1, slots[idx].len, out);
                        received_bytes += slots[idx].len;
                    } else if (slots[idx].flags & RDT_FLAG_FIN) {
                        finished = 1;
                        if (finish_time_ms == 0) {
                            finish_time_ms = rdt_now_ms();
                        }
                    }
                    slots[idx].received = 0;
                    base++;
                    if (finished) {
                        break;
                    }
                }
            }
        }
    }

    rdt_log_info("Прием завершен. Получено: %llu байт, сим.потери: %llu, плохие CRC: %llu",
                 (unsigned long long)received_bytes,
                 (unsigned long long)stats.dropped_sim,
                 (unsigned long long)stats.bad_crc);

    if (slots) {
        free(slots);
    }
    close(sockfd);
    fclose(out);
    return 0;
}
