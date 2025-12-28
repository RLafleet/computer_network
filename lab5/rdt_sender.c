#include "rdt_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
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
    int cc_enabled;
    const char *rttlog_path;
    const char *cwndlog_path;
    const char *host;
    const char *port;
    const char *file_path;
} sender_opts_t;

typedef struct {
    uint32_t seq;
    uint16_t len;
    uint16_t flags;
    const uint8_t *data;
    int acked;
    int sent;
    int retrans;
    struct timeval last_sent;
} rdt_seg_t;

static void sender_usage(const char *prog) {
    fprintf(stderr,
            "Использование: %s -alg GBN|SR [-d] [-loss pct] [-delay ms] [-corrupt pct] "
            "[-w окно] [-cc] [-rttlog файл] [-cwndlog файл] host port file\n",
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

static int parse_sender_args(int argc, char **argv, sender_opts_t *opts) {
    if (!opts) {
        return -1;
    }
    memset(opts, 0, sizeof(*opts));
    opts->window = 5;
    opts->loss_pct = 0;
    opts->delay_ms = 0;
    opts->corrupt_pct = 0;
    opts->cc_enabled = 0;

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
        } else if (strcmp(argv[i], "-cc") == 0) {
            opts->cc_enabled = 1;
            i += 1;
        } else if (strcmp(argv[i], "-rttlog") == 0 && i + 1 < argc) {
            opts->rttlog_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-cwndlog") == 0 && i + 1 < argc) {
            opts->cwndlog_path = argv[i + 1];
            i += 2;
        } else {
            break;
        }
    }

    if (i + 3 != argc) {
        return -1;
    }
    opts->host = argv[i];
    opts->port = argv[i + 1];
    opts->file_path = argv[i + 2];

    if (opts->window <= 0) {
        opts->window = 1;
    }
    return 0;
}

static int resolve_destination(const char *host, const char *port,
                               struct sockaddr_storage *out_addr, socklen_t *out_len) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0 || !res) {
        return -1;
    }

    memcpy(out_addr, res->ai_addr, res->ai_addrlen);
    *out_len = (socklen_t)res->ai_addrlen;
    freeaddrinfo(res);
    return 0;
}

static int send_segment(int sockfd, const struct sockaddr *addr, socklen_t addrlen,
                        rdt_seg_t *seg, const rdt_sim_t *sim, rdt_stats_t *stats,
                        int debug, int is_retrans) {
    uint8_t buffer[RDT_MAX_PACKET];
    size_t pkt_len = rdt_build_packet(buffer, sizeof(buffer), seg->seq, 0,
                                      seg->flags, seg->data, seg->len);
    if (pkt_len == 0) {
        return -1;
    }

    int rc = rdt_sim_sendto(sockfd, buffer, pkt_len, 0, addr, addrlen, sim, stats, debug);
    if (rc < 0) {
        return -1;
    }
    if (rc >= 0) {
        gettimeofday(&seg->last_sent, NULL);
        seg->sent = 1;
        if (is_retrans) {
            seg->retrans++;
            if (stats) {
                stats->resent_packets++;
            }
        }
        if (rc > 0) {
            rdt_log_debug(debug, "[отправка] seq=%u flags=%s len=%u", seg->seq,
                          rdt_flags_str(seg->flags), seg->len);
        }
    }

    return 0;
}

static void update_progress(uint64_t now_ms, uint64_t start_ms, uint64_t last_report,
                            uint64_t acked_bytes, uint64_t total_bytes,
                            const rdt_stats_t *stats) {
    if (now_ms - last_report < 1000) {
        return;
    }
    double percent = total_bytes > 0 ? (acked_bytes * 100.0) / (double)total_bytes : 0.0;
    double elapsed = (now_ms - start_ms) / 1000.0;
    double speed = elapsed > 0 ? (acked_bytes / 1024.0) / elapsed : 0.0;
    fprintf(stderr,
            "Прогресс: %.1f%%, скорость: %.2f КБ/с, сим.потери: %llu, ретрансляции: %llu\n",
            percent, speed, (unsigned long long)(stats ? stats->dropped_sim : 0),
            (unsigned long long)(stats ? stats->resent_packets : 0));
}

static void log_cwnd(FILE *fp, double cwnd, double ssthresh, const char *event) {
    if (!fp) {
        return;
    }
    fprintf(fp, "%llu %.3f %.3f %s\n",
            (unsigned long long)rdt_now_ms(), cwnd, ssthresh, event);
    fflush(fp);
}

static void cc_on_ack(double *cwnd, double *ssthresh, int max_window) {
    if (*cwnd < *ssthresh) {
        *cwnd += 1.0;
    } else {
        *cwnd += 1.0 / (*cwnd > 0 ? *cwnd : 1.0);
    }
    if (*cwnd > max_window) {
        *cwnd = (double)max_window;
    }
    if (*cwnd < 1.0) {
        *cwnd = 1.0;
    }
}

static void cc_on_loss(double *cwnd, double *ssthresh) {
    *ssthresh = *cwnd / 2.0;
    if (*ssthresh < 1.0) {
        *ssthresh = 1.0;
    }
    *cwnd = 1.0;
}

int main(int argc, char **argv) {
    sender_opts_t opts;
    if (parse_sender_args(argc, argv, &opts) != 0) {
        sender_usage(argv[0]);
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    FILE *fp = fopen(opts.file_path, "rb");
    if (!fp) {
        perror("Не удалось открыть файл");
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }
    long size = ftell(fp);
    if (size < 0) {
        perror("ftell");
        fclose(fp);
        return 1;
    }
    rewind(fp);

    uint64_t file_size = (uint64_t)size;
    uint8_t *file_buf = NULL;
    if (file_size > 0) {
        file_buf = (uint8_t *)malloc(file_size);
        if (!file_buf) {
            rdt_log_info("Ошибка: недостаточно памяти для файла");
            fclose(fp);
            return 1;
        }
        size_t read_bytes = fread(file_buf, 1, file_size, fp);
        if (read_bytes != file_size) {
            rdt_log_info("Ошибка чтения файла");
            free(file_buf);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);

    /* Разбиение файла на сегменты фиксированного размера */
    uint32_t data_packets = (file_size + RDT_MAX_DATA - 1) / RDT_MAX_DATA;
    uint32_t total_segments = data_packets + 2;

    rdt_seg_t *segs = (rdt_seg_t *)calloc(total_segments, sizeof(rdt_seg_t));
    if (!segs) {
        rdt_log_info("Ошибка: недостаточно памяти для сегментов");
        free(file_buf);
        return 1;
    }

    uint8_t *syn_payload = (uint8_t *)malloc(8);
    if (!syn_payload) {
        rdt_log_info("Ошибка: недостаточно памяти для SYN");
        free(segs);
        free(file_buf);
        return 1;
    }
    uint64_t size_net = rdt_hton64(file_size);
    memcpy(syn_payload, &size_net, sizeof(size_net));

    segs[0].seq = 0;
    segs[0].flags = RDT_FLAG_SYN;
    segs[0].len = 8;
    segs[0].data = syn_payload;

    for (uint32_t i = 0; i < data_packets; ++i) {
        uint32_t seq = i + 1;
        uint32_t offset = i * RDT_MAX_DATA;
        uint16_t len = (uint16_t)((file_size - offset) > RDT_MAX_DATA ? RDT_MAX_DATA
                                                                      : (file_size - offset));
        segs[seq].seq = seq;
        segs[seq].flags = RDT_FLAG_DATA;
        segs[seq].len = len;
        segs[seq].data = file_buf + offset;
    }

    segs[total_segments - 1].seq = total_segments - 1;
    segs[total_segments - 1].flags = RDT_FLAG_FIN;
    segs[total_segments - 1].len = 0;
    segs[total_segments - 1].data = NULL;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        free(syn_payload);
        free(segs);
        free(file_buf);
        return 1;
    }

    struct sockaddr_storage dest_addr;
    socklen_t dest_len = 0;
    if (resolve_destination(opts.host, opts.port, &dest_addr, &dest_len) != 0) {
        rdt_log_info("Не удалось разрешить адрес получателя");
        close(sockfd);
        free(syn_payload);
        free(segs);
        free(file_buf);
        return 1;
    }

    rdt_sim_t sim = {opts.loss_pct, opts.delay_ms, opts.corrupt_pct};
    rdt_stats_t stats;
    rdt_stats_init(&stats);

    rdt_rtt_t rtt;
    rdt_rtt_init(&rtt, 300.0);

    FILE *rttlog = NULL;
    if (opts.rttlog_path) {
        rttlog = fopen(opts.rttlog_path, "w");
        if (!rttlog) {
            rdt_log_info("Не удалось открыть файл RTT-лога");
        }
    }

    FILE *cwndlog = NULL;
    if (opts.cwndlog_path) {
        cwndlog = fopen(opts.cwndlog_path, "w");
        if (!cwndlog) {
            rdt_log_info("Не удалось открыть файл лога окна");
        }
    }

    uint32_t base = 0;
    uint32_t next = 0;
    uint64_t acked_bytes = 0;
    uint64_t start_ms = rdt_now_ms();
    uint64_t last_report = start_ms;
    int timeout_count = 0;

    double cwnd = opts.cc_enabled ? 1.0 : (double)opts.window;
    double ssthresh = (double)opts.window;

    /* Основной цикл: отправка в пределах окна, ожидание ACK, таймауты */
    while (base < total_segments) {
        int window_size = opts.window;
        if (opts.cc_enabled) {
            window_size = (int)cwnd;
            if (window_size < 1) {
                window_size = 1;
            }
            if (window_size > opts.window) {
                window_size = opts.window;
            }
        }

        while (next < total_segments && next < base + (uint32_t)window_size) {
            if (send_segment(sockfd, (struct sockaddr *)&dest_addr, dest_len,
                             &segs[next], &sim, &stats, opts.debug, 0) != 0) {
                rdt_log_info("Ошибка отправки пакета");
                goto cleanup;
            }
            next++;
        }

        uint64_t now_ms = rdt_now_ms();
        update_progress(now_ms, start_ms, last_report, acked_bytes, file_size, &stats);
        if (now_ms - last_report >= 1000) {
            last_report = now_ms;
        }

        if (base == next) {
            continue;
        }

        uint64_t elapsed = rdt_elapsed_ms(&segs[base].last_sent);
        int timeout_ms = (int)(rtt.rto - (double)elapsed);
        if (timeout_ms < 0) {
            timeout_ms = 0;
        }
        if (timeout_ms > 500) {
            timeout_ms = 500;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int sel = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (sel < 0 && errno != EINTR) {
            perror("select");
            goto cleanup;
        }

        if (sel == 0) {
            timeout_count++;
            rdt_log_debug(opts.debug, "[таймаут] база=%u", base);
            if (opts.cc_enabled) {
                cc_on_loss(&cwnd, &ssthresh);
                log_cwnd(cwndlog, cwnd, ssthresh, "таймаут");
            }
            for (uint32_t i = base; i < next; ++i) {
                if (send_segment(sockfd, (struct sockaddr *)&dest_addr, dest_len,
                                 &segs[i], &sim, &stats, opts.debug, 1) != 0) {
                    rdt_log_info("Ошибка повторной отправки");
                    goto cleanup;
                }
            }
            if (timeout_count > 20) {
                rdt_log_info("Соединение потеряно (слишком много таймаутов)");
                goto cleanup;
            }
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            uint8_t recv_buf[RDT_MAX_PACKET];
            struct sockaddr_storage from;
            socklen_t from_len = sizeof(from);
            ssize_t rcv = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0,
                                   (struct sockaddr *)&from, &from_len);
            if (rcv <= 0) {
                continue;
            }
            stats.recv_packets++;

            rdt_header_t hdr;
            uint8_t payload[RDT_MAX_DATA];
            int parse_rc = rdt_parse_packet(recv_buf, (size_t)rcv, &hdr, payload, sizeof(payload));
            if (parse_rc == -2) {
                stats.bad_crc++;
                rdt_log_debug(opts.debug, "[recv] поврежденный ACK");
                continue;
            }
            if (parse_rc != 0) {
                continue;
            }

            if (!(hdr.flags & RDT_FLAG_ACK)) {
                continue;
            }
            stats.acks_recv++;

            if (opts.alg == ALG_GBN) {
                uint32_t ackno = hdr.ack;
                if (ackno > total_segments) {
                    continue;
                }
                if (ackno > base) {
                    for (uint32_t i = base; i < ackno; ++i) {
                        if (!segs[i].acked) {
                            segs[i].acked = 1;
                            if (segs[i].flags & RDT_FLAG_DATA) {
                                acked_bytes += segs[i].len;
                            }
                            if (opts.cc_enabled) {
                                cc_on_ack(&cwnd, &ssthresh, opts.window);
                                log_cwnd(cwndlog, cwnd, ssthresh, "подтверждение");
                            }
                        }
                    }
                    if (!segs[ackno - 1].retrans) {
                        uint64_t sample = rdt_elapsed_ms(&segs[ackno - 1].last_sent);
                        rdt_rtt_update(&rtt, (double)sample);
                        if (rttlog) {
                            fprintf(rttlog, "%u %llu %.2f %.2f\n", ackno - 1,
                                    (unsigned long long)sample, rtt.srtt, rtt.rto);
                            fflush(rttlog);
                        }
                    }
                    base = ackno;
                    timeout_count = 0;
                    rdt_log_debug(opts.debug, "[ACK] cumul=%u", ackno);
                } else {
                    stats.dup_acks++;
                    rdt_log_debug(opts.debug, "[дубликат ACK] %u", ackno);
                }
            } else {
                uint32_t ackno = hdr.ack;
                if (ackno >= total_segments) {
                    continue;
                }
                if (!segs[ackno].acked) {
                    segs[ackno].acked = 1;
                    if (segs[ackno].flags & RDT_FLAG_DATA) {
                        acked_bytes += segs[ackno].len;
                    }
                    if (opts.cc_enabled) {
                        cc_on_ack(&cwnd, &ssthresh, opts.window);
                        log_cwnd(cwndlog, cwnd, ssthresh, "подтверждение");
                    }
                    if (!segs[ackno].retrans) {
                        uint64_t sample = rdt_elapsed_ms(&segs[ackno].last_sent);
                        rdt_rtt_update(&rtt, (double)sample);
                        if (rttlog) {
                            fprintf(rttlog, "%u %llu %.2f %.2f\n", ackno,
                                    (unsigned long long)sample, rtt.srtt, rtt.rto);
                            fflush(rttlog);
                        }
                    }
                    rdt_log_debug(opts.debug, "[ACK] seq=%u", ackno);
                } else {
                    stats.dup_acks++;
                }

                while (base < total_segments && segs[base].acked) {
                    base++;
                }
            }
        }

        if (opts.alg == ALG_SR) {
            uint64_t now = rdt_now_ms();
            for (uint32_t i = base; i < next; ++i) {
                if (segs[i].acked || !segs[i].sent) {
                    continue;
                }
                uint64_t elapsed_ms = rdt_elapsed_ms(&segs[i].last_sent);
                if (elapsed_ms >= (uint64_t)rtt.rto) {
                    timeout_count++;
                    rdt_log_debug(opts.debug, "[таймаут SR] seq=%u", segs[i].seq);
                    if (opts.cc_enabled) {
                        cc_on_loss(&cwnd, &ssthresh);
                        log_cwnd(cwndlog, cwnd, ssthresh, "таймаут");
                    }
                    if (send_segment(sockfd, (struct sockaddr *)&dest_addr, dest_len,
                                     &segs[i], &sim, &stats, opts.debug, 1) != 0) {
                        rdt_log_info("Ошибка повторной отправки");
                        goto cleanup;
                    }
                    if (timeout_count > 20) {
                        rdt_log_info("Соединение потеряно (слишком много таймаутов)");
                        goto cleanup;
                    }
                }
            }
            (void)now;
        }
    }

    rdt_log_info("Передача завершена. Отправлено: %llu байт, ретрансляции: %llu, сим.потери: %llu",
                 (unsigned long long)file_size,
                 (unsigned long long)stats.resent_packets,
                 (unsigned long long)stats.dropped_sim);

cleanup:
    if (rttlog) {
        fclose(rttlog);
    }
    if (cwndlog) {
        fclose(cwndlog);
    }
    close(sockfd);
    free(syn_payload);
    free(segs);
    free(file_buf);
    return 0;
}
