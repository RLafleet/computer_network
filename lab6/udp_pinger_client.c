#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PING_COUNT 10
#define BUF_SIZE 256

static volatile sig_atomic_t keep_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

static void usage(const char *prog) {
    printf("Использование: %s <server_host> <server_port> [опции]\n", prog);
    printf("Опции:\n");
    printf("  -d              подробный вывод отладки\n");
    printf("  -stats          вывести статистику после 10 запросов\n");
    printf("  -hb             включить heartbeat после ping\n");
    printf("  -hb-interval N  интервал heartbeat в секундах (по умолчанию 2)\n");
}

typedef struct {
    const char *host;
    const char *port;
    int debug;
    int stats;
    int heartbeat;
    int hb_interval_ms;
} client_config;

static int parse_args(int argc, char **argv, client_config *cfg) {
    int i;

    memset(cfg, 0, sizeof(*cfg));
    cfg->hb_interval_ms = 2000;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            cfg->debug = 1;
        } else if (strcmp(argv[i], "-stats") == 0) {
            cfg->stats = 1;
        } else if (strcmp(argv[i], "-hb") == 0) {
            cfg->heartbeat = 1;
        } else if (strcmp(argv[i], "-hb-interval") == 0) {
            if (i + 1 >= argc) {
                return -1;
            }
            cfg->hb_interval_ms = atoi(argv[++i]) * 1000;
            if (cfg->hb_interval_ms <= 0) {
                return -1;
            }
        } else if (argv[i][0] == '-') {
            return -1;
        } else if (cfg->host == NULL) {
            cfg->host = argv[i];
        } else if (cfg->port == NULL) {
            cfg->port = argv[i];
        } else {
            return -1;
        }
    }

    if (cfg->host == NULL || cfg->port == NULL) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    client_config cfg;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sockfd = -1;
    struct timeval timeout;
    char sendbuf[BUF_SIZE];
    char recvbuf[BUF_SIZE];
    int i;
    int received = 0;
    long long rtt_min_ms = 0;
    long long rtt_max_ms = 0;
    long long rtt_sum_ms = 0;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(cfg.host, cfg.port, &hints, &res) != 0) {
        fprintf(stderr, "Ошибка: не удалось разрешить адрес сервера\n");
        return 1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        fprintf(stderr, "Ошибка socket: %s\n", strerror(errno));
        freeaddrinfo(res);
        return 1;
    }

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Ошибка setsockopt: %s\n", strerror(errno));
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }

    if (cfg.debug) {
        char addr_str[INET_ADDRSTRLEN];
        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
        printf("Цель: %s:%d\n", addr_str, ntohs(sin->sin_port));
    }

    for (i = 1; i <= PING_COUNT; i++) {
        long long send_ts = now_ms();
        long long recv_ts;
        long long rtt_ms;
        ssize_t n;
        int reply_seq = i;
        long long reply_ts = send_ts;
        double rtt_sec;

        snprintf(sendbuf, sizeof(sendbuf), "Ping %d %lld", i, send_ts);

        if (cfg.debug) {
            printf("Отправка: %s\n", sendbuf);
        }

        if (sendto(sockfd, sendbuf, strlen(sendbuf), 0, res->ai_addr, res->ai_addrlen) < 0) {
            fprintf(stderr, "Ошибка sendto: %s\n", strerror(errno));
            continue;
        }

        n = recvfrom(sockfd, recvbuf, sizeof(recvbuf) - 1, 0, NULL, NULL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Таймаут ожидания\n");
            } else {
                fprintf(stderr, "Ошибка recvfrom: %s\n", strerror(errno));
            }
        } else {
            recvbuf[n] = '\0';
            recv_ts = now_ms();
            if (sscanf(recvbuf, "Ping %d %lld", &reply_seq, &reply_ts) != 2) {
                reply_seq = i;
                reply_ts = send_ts;
            }

            rtt_ms = recv_ts - send_ts;
            rtt_sec = (double)rtt_ms / 1000.0;
            printf("PING %d %lld, RTT = %.3f сек\n", reply_seq, reply_ts, rtt_sec);

            received++;
            if (received == 1) {
                rtt_min_ms = rtt_ms;
                rtt_max_ms = rtt_ms;
            } else {
                if (rtt_ms < rtt_min_ms) {
                    rtt_min_ms = rtt_ms;
                }
                if (rtt_ms > rtt_max_ms) {
                    rtt_max_ms = rtt_ms;
                }
            }
            rtt_sum_ms += rtt_ms;
        }

        sleep_ms(1000);
    }

    if (cfg.stats) {
        double loss_pct = ((double)(PING_COUNT - received) * 100.0) / (double)PING_COUNT;
        printf("--- Статистика ping ---\n");
        printf("Отправлено: %d, Получено: %d, Потеряно: %d (%.0f%%)\n",
               PING_COUNT, received, PING_COUNT - received, loss_pct);
        if (received > 0) {
            double rtt_min_sec = (double)rtt_min_ms / 1000.0;
            double rtt_max_sec = (double)rtt_max_ms / 1000.0;
            double rtt_avg_sec = ((double)rtt_sum_ms / (double)received) / 1000.0;
            printf("RTT: мин = %.3fс, макс = %.3fс, сред = %.3fс\n",
                   rtt_min_sec, rtt_max_sec, rtt_avg_sec);
        } else {
            printf("RTT: нет данных\n");
        }
    }

    if (cfg.heartbeat) {
        signal(SIGINT, handle_sigint);
        printf("Режим heartbeat включен, интервал %d мс. Нажмите Ctrl+C для выхода.\n",
               cfg.hb_interval_ms);
        while (keep_running) {
            long long ts = now_ms();
            snprintf(sendbuf, sizeof(sendbuf), "Heartbeat %lld", ts);
            if (cfg.debug) {
                printf("Heartbeat отправлен: %s\n", sendbuf);
            }
            if (sendto(sockfd, sendbuf, strlen(sendbuf), 0, res->ai_addr, res->ai_addrlen) < 0) {
                fprintf(stderr, "Ошибка sendto (heartbeat): %s\n", strerror(errno));
            }
            if (recvfrom(sockfd, recvbuf, sizeof(recvbuf) - 1, MSG_DONTWAIT, NULL, NULL) > 0) {
                if (cfg.debug) {
                    printf("Heartbeat-ответ получен\n");
                }
            }
            sleep_ms(cfg.hb_interval_ms);
        }
    }

    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
