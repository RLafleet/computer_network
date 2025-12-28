#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 256

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static void usage(const char *prog) {
    printf("Использование: %s <port> [опции]\n", prog);
    printf("Опции:\n");
    printf("  -loss N         вероятность потери пакетов (0..100)\n");
    printf("  -hb-timeout N   таймаут heartbeat в секундах (по умолчанию 5)\n");
    printf("  -d              подробный вывод отладки\n");
}

typedef struct {
    int port;
    int loss_pct;
    int hb_timeout_sec;
    int debug;
} server_config;

static int parse_args(int argc, char **argv, server_config *cfg) {
    int i;

    memset(cfg, 0, sizeof(*cfg));
    cfg->hb_timeout_sec = 5;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-loss") == 0) {
            if (i + 1 >= argc) {
                return -1;
            }
            cfg->loss_pct = atoi(argv[++i]);
            if (cfg->loss_pct < 0 || cfg->loss_pct > 100) {
                return -1;
            }
        } else if (strcmp(argv[i], "-hb-timeout") == 0) {
            if (i + 1 >= argc) {
                return -1;
            }
            cfg->hb_timeout_sec = atoi(argv[++i]);
            if (cfg->hb_timeout_sec <= 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            cfg->debug = 1;
        } else if (argv[i][0] == '-') {
            return -1;
        } else if (cfg->port == 0) {
            cfg->port = atoi(argv[i]);
            if (cfg->port <= 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    if (cfg->port == 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    server_config cfg;
    int sockfd;
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    long long last_packet_ms = 0;
    int has_last = 0;
    int inactive_reported = 0;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    srand((unsigned int)time(NULL));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Ошибка socket: %s\n", strerror(errno));
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((unsigned short)cfg.port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Ошибка bind: %s\n", strerror(errno));
        close(sockfd);
        return 1;
    }

    printf("UDP Pinger сервер запущен на порту %d\n", cfg.port);
    if (cfg.loss_pct > 0) {
        printf("Симуляция потерь: %d%%\n", cfg.loss_pct);
    }
    printf("Таймаут heartbeat: %d сек\n", cfg.hb_timeout_sec);

    while (1) {
        fd_set readfds;
        struct timeval tv;
        int sel;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        sel = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Ошибка select: %s\n", strerror(errno));
            break;
        } else if (sel == 0) {
            if (has_last) {
                long long diff_ms = now_ms() - last_packet_ms;
                if (!inactive_reported && diff_ms >= (long long)cfg.hb_timeout_sec * 1000LL) {
                    printf("Предупреждение: клиент неактивен более %d сек\n", cfg.hb_timeout_sec);
                    inactive_reported = 1;
                }
            }
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            client_len = sizeof(client_addr);
            ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                                 (struct sockaddr *)&client_addr, &client_len);
            if (n < 0) {
                fprintf(stderr, "Ошибка recvfrom: %s\n", strerror(errno));
                continue;
            }

            buf[n] = '\0';
            last_packet_ms = now_ms();
            has_last = 1;
            if (inactive_reported) {
                printf("Клиент снова активен\n");
                inactive_reported = 0;
            }

            if (cfg.debug) {
                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
                printf("Получен пакет от %s:%d: %s\n",
                       addr_str, ntohs(client_addr.sin_port), buf);
            }

            if (cfg.loss_pct > 0 && (rand() % 100) < cfg.loss_pct) {
                if (cfg.debug) {
                    printf("Пакет потерян (симуляция)\n");
                }
                continue;
            }

            if (sendto(sockfd, buf, n, 0, (struct sockaddr *)&client_addr, client_len) < 0) {
                fprintf(stderr, "Ошибка sendto: %s\n", strerror(errno));
            }
        }
    }

    close(sockfd);
    return 0;
}
