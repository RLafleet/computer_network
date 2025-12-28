#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_HEADER_SIZE (64 * 1024)
#define MAX_HEADERS 128
#define MAX_LINE 4096
#define MAX_BODY_SIZE (10 * 1024 * 1024)
#define IO_BUF_SIZE 4096

typedef struct {
    char name[128];
    char value[2048];
} header_t;

typedef struct {
    char method[8];
    char url[2048];
    char version[16];
    char host[256];
    int port;
    char path[2048];
    header_t headers[MAX_HEADERS];
    int header_count;
    int content_length;
    char *body;
    size_t body_len;
} http_request_t;

typedef struct {
    int status_code;
    int has_max_age;
    int max_age;
    int has_expires;
    time_t expires;
    int no_store;
    int no_cache;
    int must_revalidate;
    int has_last_modified;
    char last_modified[128];
    int has_etag;
    char etag[128];
} http_response_info_t;

typedef struct {
    time_t stored_at;
    time_t expires;
    int must_revalidate;
    char last_modified[128];
    char etag[128];
} cache_meta_t;

typedef struct {
    char cache_dir[PATH_MAX];
    int debug;
} proxy_config_t;

typedef struct {
    int client_fd;
    proxy_config_t *config;
} client_ctx_t;

static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static void log_msg(const proxy_config_t *cfg, const char *level, const char *fmt, ...) {
    if (strcmp(level, "DEBUG") == 0 && !cfg->debug) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(stderr, "[%s] %s: ", ts, level);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void usage(const char *prog) {
    fprintf(stderr, "Использование: %s <порт> [-cache_dir путь] [-d]\n", prog);
}

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static void send_simple_response(int fd, int status, const char *reason, const char *body) {
    char header[512];
    size_t body_len = body ? strlen(body) : 0;
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.0 %d %s\r\n"
                     "Content-Type: text/plain; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     status, reason, body_len);
    if (n > 0) {
        send_all(fd, header, (size_t)n);
    }
    if (body_len > 0) {
        send_all(fd, body, body_len);
    }
}

static int find_header_end(const char *buf, size_t len) {
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return (int)(i + 4);
        }
    }
    return -1;
}

static int recv_header(int fd, char **out_buf, size_t *out_len, size_t *header_len, char *err, size_t errsz) {
    size_t cap = 8192;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, errsz, "нет памяти");
        return -1;
    }

    while (1) {
        if (len == cap) {
            cap *= 2;
            if (cap > MAX_HEADER_SIZE) {
                free(buf);
                snprintf(err, errsz, "слишком большие заголовки");
                return -1;
            }
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) {
                free(buf);
                snprintf(err, errsz, "нет памяти");
                return -1;
            }
            buf = tmp;
        }

        ssize_t n = recv(fd, buf + len, cap - len, 0);
        if (n == 0) {
            free(buf);
            snprintf(err, errsz, "соединение закрыто");
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buf);
            snprintf(err, errsz, "ошибка чтения: %s", strerror(errno));
            return -1;
        }
        len += (size_t)n;

        int idx = find_header_end(buf, len);
        if (idx >= 0) {
            *out_buf = buf;
            *out_len = len;
            *header_len = (size_t)idx;
            return 0;
        }

        if (len > MAX_HEADER_SIZE) {
            free(buf);
            snprintf(err, errsz, "слишком большие заголовки");
            return -1;
        }
    }
}

static void trim(char *s) {
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) {
        start++;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) {
        end--;
    }
    if (start > 0) {
        memmove(s, s + start, end - start);
    }
    s[end - start] = '\0';
}

static void copy_str(char *dst, size_t dst_sz, const char *src) {
    if (dst_sz == 0) {
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_sz) {
        len = dst_sz - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int parse_request_line(const char *line, http_request_t *req, char *err, size_t errsz) {
    const char *sp1 = strchr(line, ' ');
    if (!sp1) {
        snprintf(err, errsz, "некорректная строка запроса");
        return -1;
    }
    const char *sp2 = strchr(sp1 + 1, ' ');
    if (!sp2) {
        snprintf(err, errsz, "некорректная строка запроса");
        return -1;
    }

    size_t mlen = (size_t)(sp1 - line);
    size_t ulen = (size_t)(sp2 - (sp1 + 1));
    size_t vlen = strlen(sp2 + 1);

    if (mlen >= sizeof(req->method) || ulen >= sizeof(req->url) || vlen >= sizeof(req->version)) {
        snprintf(err, errsz, "слишком длинная строка запроса");
        return -1;
    }

    memcpy(req->method, line, mlen);
    req->method[mlen] = '\0';
    memcpy(req->url, sp1 + 1, ulen);
    req->url[ulen] = '\0';
    memcpy(req->version, sp2 + 1, vlen);
    req->version[vlen] = '\0';

    return 0;
}

static int parse_headers(const char *buf, size_t header_len, http_request_t *req) {
    const char *p = buf;
    const char *end = buf + header_len;

    const char *line_end = strstr(p, "\r\n");
    if (!line_end) {
        return -1;
    }

    char line[MAX_LINE];
    size_t line_len = (size_t)(line_end - p);
    if (line_len >= sizeof(line)) {
        return -1;
    }
    memcpy(line, p, line_len);
    line[line_len] = '\0';

    char err[128];
    if (parse_request_line(line, req, err, sizeof(err)) != 0) {
        return -1;
    }

    p = line_end + 2;
    while (p < end) {
        const char *eol = strstr(p, "\r\n");
        if (!eol) {
            break;
        }
        if (eol == p) {
            break;
        }

        size_t len = (size_t)(eol - p);
        if (len >= sizeof(line)) {
            return -1;
        }
        memcpy(line, p, len);
        line[len] = '\0';

        char *colon = strchr(line, ':');
        if (colon && req->header_count < MAX_HEADERS) {
            *colon = '\0';
            char *name = line;
            char *value = colon + 1;
            trim(name);
            trim(value);

            copy_str(req->headers[req->header_count].name, sizeof(req->headers[req->header_count].name), name);
            copy_str(req->headers[req->header_count].value, sizeof(req->headers[req->header_count].value), value);
            req->header_count++;
        }

        p = eol + 2;
    }

    return 0;
}

static const char *find_header_value(const http_request_t *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

static int parse_host_port(const char *hostport, char *host, size_t hostsz, int *port) {
    const char *colon = strrchr(hostport, ':');
    if (colon && strchr(colon + 1, ']') == NULL) {
        size_t hlen = (size_t)(colon - hostport);
        if (hlen >= hostsz) {
            return -1;
        }
        memcpy(host, hostport, hlen);
        host[hlen] = '\0';
        int p = atoi(colon + 1);
        if (p <= 0 || p > 65535) {
            return -1;
        }
        *port = p;
        return 0;
    }

    if (strlen(hostport) >= hostsz) {
        return -1;
    }
    strcpy(host, hostport);
    *port = 80;
    return 0;
}

static int parse_url(http_request_t *req, char *err, size_t errsz) {
    const char *url = req->url;
    if (strncmp(url, "/http://", 8) == 0) {
        url++;
    }

    if (strncasecmp(url, "http://", 7) == 0) {
        const char *host_start = url + 7;
        const char *path_start = strchr(host_start, '/');
        char hostport[256];

        if (path_start) {
            size_t hlen = (size_t)(path_start - host_start);
            if (hlen >= sizeof(hostport)) {
                snprintf(err, errsz, "слишком длинный хост");
                return -1;
            }
            memcpy(hostport, host_start, hlen);
            hostport[hlen] = '\0';
            strncpy(req->path, path_start, sizeof(req->path) - 1);
            req->path[sizeof(req->path) - 1] = '\0';
        } else {
            strncpy(hostport, host_start, sizeof(hostport) - 1);
            hostport[sizeof(hostport) - 1] = '\0';
            strcpy(req->path, "/");
        }

        if (parse_host_port(hostport, req->host, sizeof(req->host), &req->port) != 0) {
            snprintf(err, errsz, "некорректный хост");
            return -1;
        }
        return 0;
    }

    if (strncasecmp(url, "https://", 8) == 0) {
        snprintf(err, errsz, "HTTPS не поддерживается");
        return -1;
    }

    if (url[0] == '/') {
        const char *host_header = find_header_value(req, "Host");
        if (!host_header) {
            snprintf(err, errsz, "нет Host заголовка");
            return -1;
        }
        if (parse_host_port(host_header, req->host, sizeof(req->host), &req->port) != 0) {
            snprintf(err, errsz, "некорректный Host");
            return -1;
        }
        strncpy(req->path, url, sizeof(req->path) - 1);
        req->path[sizeof(req->path) - 1] = '\0';
        return 0;
    }

    snprintf(err, errsz, "некорректный URL");
    return -1;
}

static int read_request(int fd, http_request_t *req, char *err, size_t errsz) {
    memset(req, 0, sizeof(*req));

    char *buf = NULL;
    size_t total_len = 0;
    size_t header_len = 0;

    if (recv_header(fd, &buf, &total_len, &header_len, err, errsz) != 0) {
        return -1;
    }

    if (parse_headers(buf, header_len, req) != 0) {
        free(buf);
        snprintf(err, errsz, "ошибка разбора заголовков");
        return -1;
    }

    const char *cl = find_header_value(req, "Content-Length");
    if (cl) {
        req->content_length = atoi(cl);
        if (req->content_length < 0 || req->content_length > MAX_BODY_SIZE) {
            free(buf);
            snprintf(err, errsz, "слишком большое тело запроса");
            return -1;
        }
    }

    if (strcasecmp(req->method, "POST") == 0 && req->content_length > 0) {
        req->body_len = (size_t)req->content_length;
        req->body = (char *)malloc(req->body_len);
        if (!req->body) {
            free(buf);
            snprintf(err, errsz, "нет памяти");
            return -1;
        }
        size_t have = total_len > header_len ? total_len - header_len : 0;
        if (have > req->body_len) {
            have = req->body_len;
        }
        if (have > 0) {
            memcpy(req->body, buf + header_len, have);
        }
        size_t remaining = req->body_len - have;
        size_t offset = have;
        while (remaining > 0) {
            ssize_t n = recv(fd, req->body + offset, remaining, 0);
            if (n <= 0) {
                free(buf);
                snprintf(err, errsz, "ошибка чтения тела запроса");
                return -1;
            }
            offset += (size_t)n;
            remaining -= (size_t)n;
        }
    }

    free(buf);

    if (parse_url(req, err, errsz) != 0) {
        return -1;
    }

    return 0;
}

static bool is_hop_by_hop_header(const char *name) {
    return strcasecmp(name, "Connection") == 0 ||
           strcasecmp(name, "Proxy-Connection") == 0 ||
           strcasecmp(name, "Keep-Alive") == 0 ||
           strcasecmp(name, "Transfer-Encoding") == 0 ||
           strcasecmp(name, "TE") == 0 ||
           strcasecmp(name, "Trailer") == 0 ||
           strcasecmp(name, "Upgrade") == 0;
}

static int append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t slen = strlen(s);
    if (*len + slen + 1 > *cap) {
        size_t new_cap = (*cap) * 2 + slen + 1;
        char *tmp = (char *)realloc(*buf, new_cap);
        if (!tmp) {
            return -1;
        }
        *buf = tmp;
        *cap = new_cap;
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
    return 0;
}

static int append_fmt(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    if ((size_t)n >= sizeof(tmp)) {
        return -1;
    }
    return append_str(buf, len, cap, tmp);
}

static int build_forward_request(const http_request_t *req, const char *cond_headers,
                                 char **out_buf, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        return -1;
    }
    buf[0] = '\0';

    if (append_fmt(&buf, &len, &cap, "%s %s HTTP/1.0\r\n", req->method, req->path) != 0) {
        free(buf);
        return -1;
    }
    if (append_fmt(&buf, &len, &cap, "Host: %s\r\n", req->host) != 0) {
        free(buf);
        return -1;
    }

    bool has_user_agent = false;
    bool has_content_length = false;

    for (int i = 0; i < req->header_count; i++) {
        const char *name = req->headers[i].name;
        const char *value = req->headers[i].value;

        if (strcasecmp(name, "Host") == 0) {
            continue;
        }
        if (strcasecmp(name, "User-Agent") == 0) {
            has_user_agent = true;
        }
        if (strcasecmp(name, "Content-Length") == 0) {
            has_content_length = true;
        }
        if (strcasecmp(name, "If-Modified-Since") == 0 || strcasecmp(name, "If-None-Match") == 0) {
            continue;
        }
        if (is_hop_by_hop_header(name)) {
            continue;
        }

        if (append_fmt(&buf, &len, &cap, "%s: %s\r\n", name, value) != 0) {
            free(buf);
            return -1;
        }
    }

    if (!has_user_agent) {
        if (append_str(&buf, &len, &cap, "User-Agent: ProxyLab/1.0\r\n") != 0) {
            free(buf);
            return -1;
        }
    }

    if (strcasecmp(req->method, "POST") == 0 && !has_content_length) {
        if (append_fmt(&buf, &len, &cap, "Content-Length: %zu\r\n", req->body_len) != 0) {
            free(buf);
            return -1;
        }
    }

    if (cond_headers && cond_headers[0] != '\0') {
        if (append_str(&buf, &len, &cap, cond_headers) != 0) {
            free(buf);
            return -1;
        }
    }

    if (append_str(&buf, &len, &cap, "Connection: close\r\n\r\n") != 0) {
        free(buf);
        return -1;
    }

    *out_buf = buf;
    *out_len = len;
    return 0;
}

static int connect_to_host(const char *host, int port, char *err, size_t errsz) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) {
        snprintf(err, errsz, "getaddrinfo: %s", gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    snprintf(err, errsz, "не удалось подключиться к %s:%d", host, port);
    return -1;
}

static time_t timegm_compat(struct tm *tm) {
#if defined(_GNU_SOURCE)
    return timegm(tm);
#else
    char *old_tz = getenv("TZ");
    setenv("TZ", "UTC", 1);
    tzset();
    time_t t = mktime(tm);
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return t;
#endif
}

static int parse_http_date(const char *value, time_t *out) {
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    char *res = strptime(value, "%a, %d %b %Y %H:%M:%S GMT", &tm_val);
    if (!res) {
        return -1;
    }
    *out = timegm_compat(&tm_val);
    return 0;
}

static void parse_cache_control(const char *value, http_response_info_t *info) {
    char buf[1024];
    size_t len = strlen(value);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)value[i]);
    }
    buf[len] = '\0';

    if (strstr(buf, "no-store")) {
        info->no_store = 1;
    }
    if (strstr(buf, "no-cache")) {
        info->no_cache = 1;
        info->must_revalidate = 1;
    }
    if (strstr(buf, "must-revalidate")) {
        info->must_revalidate = 1;
    }

    const char *p = strstr(buf, "max-age=");
    if (p) {
        p += 8;
        int max_age = atoi(p);
        if (max_age >= 0) {
            info->has_max_age = 1;
            info->max_age = max_age;
        }
    }
}

static int parse_response_info(const char *buf, size_t header_len, http_response_info_t *info) {
    memset(info, 0, sizeof(*info));

    const char *p = buf;
    const char *end = buf + header_len;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end) {
        return -1;
    }
    char line[MAX_LINE];
    size_t line_len = (size_t)(line_end - p);
    if (line_len >= sizeof(line)) {
        return -1;
    }
    memcpy(line, p, line_len);
    line[line_len] = '\0';

    int code = 0;
    if (sscanf(line, "HTTP/%*s %d", &code) != 1) {
        return -1;
    }
    info->status_code = code;

    p = line_end + 2;
    while (p < end) {
        const char *eol = strstr(p, "\r\n");
        if (!eol || eol == p) {
            break;
        }
        size_t len = (size_t)(eol - p);
        if (len >= sizeof(line)) {
            return -1;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *name = line;
            char *value = colon + 1;
            trim(name);
            trim(value);

            if (strcasecmp(name, "Cache-Control") == 0) {
                parse_cache_control(value, info);
            } else if (strcasecmp(name, "Expires") == 0) {
                time_t t;
                if (parse_http_date(value, &t) == 0) {
                    info->has_expires = 1;
                    info->expires = t;
                }
            } else if (strcasecmp(name, "Last-Modified") == 0) {
                info->has_last_modified = 1;
                copy_str(info->last_modified, sizeof(info->last_modified), value);
            } else if (strcasecmp(name, "ETag") == 0) {
                info->has_etag = 1;
                copy_str(info->etag, sizeof(info->etag), value);
            }
        }
        p = eol + 2;
    }

    return 0;
}

static uint64_t fnv1a_hash(const char *s) {
    uint64_t hash = 1469598103934665603ULL;
    while (*s) {
        hash ^= (unsigned char)(*s);
        hash *= 1099511628211ULL;
        s++;
    }
    return hash;
}

static void cache_key_for_url(const char *url, char *out, size_t out_sz) {
    uint64_t h = fnv1a_hash(url);
    snprintf(out, out_sz, "%016llx", (unsigned long long)h);
}

static int cache_meta_read(const char *path, cache_meta_t *meta) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    memset(meta, 0, sizeof(*meta));

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "stored_at") == 0) {
            meta->stored_at = (time_t)atoll(val);
        } else if (strcmp(key, "expires") == 0) {
            meta->expires = (time_t)atoll(val);
        } else if (strcmp(key, "must_revalidate") == 0) {
            meta->must_revalidate = atoi(val);
        } else if (strcmp(key, "last_modified") == 0) {
            strncpy(meta->last_modified, val, sizeof(meta->last_modified) - 1);
            meta->last_modified[sizeof(meta->last_modified) - 1] = '\0';
        } else if (strcmp(key, "etag") == 0) {
            strncpy(meta->etag, val, sizeof(meta->etag) - 1);
            meta->etag[sizeof(meta->etag) - 1] = '\0';
        }
    }

    fclose(f);
    return 0;
}

static int cache_meta_write(const char *path, const cache_meta_t *meta) {
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        return -1;
    }

    fprintf(f, "stored_at=%lld\n", (long long)meta->stored_at);
    fprintf(f, "expires=%lld\n", (long long)meta->expires);
    fprintf(f, "must_revalidate=%d\n", meta->must_revalidate);
    if (meta->last_modified[0]) {
        fprintf(f, "last_modified=%s\n", meta->last_modified);
    }
    if (meta->etag[0]) {
        fprintf(f, "etag=%s\n", meta->etag);
    }

    fclose(f);
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

static int send_file(int fd, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    char buf[IO_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (send_all(fd, buf, n) != 0) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

static int recv_response_header(int fd, char **out_buf, size_t *out_len, size_t *header_len, char *err, size_t errsz) {
    return recv_header(fd, out_buf, out_len, header_len, err, errsz);
}

static int ensure_cache_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }
    if (mkdir(path, 0755) != 0) {
        return -1;
    }
    return 0;
}

static int join_path(char *out, size_t out_sz, const char *dir, const char *name, const char *suffix) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    size_t need = dir_len + 1 + name_len + suffix_len + 1;
    if (need > out_sz) {
        return -1;
    }

    size_t pos = 0;
    memcpy(out + pos, dir, dir_len);
    pos += dir_len;
    if (pos == 0 || out[pos - 1] != '/') {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, name_len);
    pos += name_len;
    memcpy(out + pos, suffix, suffix_len);
    pos += suffix_len;
    out[pos] = '\0';
    return 0;
}

static int build_cache_paths(const proxy_config_t *cfg, const char *url,
                             char *cache_path, size_t cache_sz,
                             char *meta_path, size_t meta_sz,
                             char *key_out, size_t key_sz) {
    cache_key_for_url(url, key_out, key_sz);
    if (join_path(cache_path, cache_sz, cfg->cache_dir, key_out, ".cache") != 0) {
        return -1;
    }
    if (join_path(meta_path, meta_sz, cfg->cache_dir, key_out, ".meta") != 0) {
        return -1;
    }
    return 0;
}

static int cache_is_fresh(const cache_meta_t *meta, time_t now) {
    if (meta->must_revalidate) {
        return 0;
    }
    if (meta->expires == 0) {
        return 0;
    }
    return now < meta->expires;
}

static int try_serve_cache(const proxy_config_t *cfg, const char *cache_path, const char *meta_path,
                           cache_meta_t *meta, int *has_meta, int client_fd) {
    struct stat st_cache;
    struct stat st_meta;

    pthread_mutex_lock(&cache_mutex);
    int cache_ok = (stat(cache_path, &st_cache) == 0);
    int meta_ok = (stat(meta_path, &st_meta) == 0);
    if (cache_ok && meta_ok && cache_meta_read(meta_path, meta) == 0) {
        *has_meta = 1;
    } else {
        *has_meta = 0;
    }
    pthread_mutex_unlock(&cache_mutex);

    if (!cache_ok || !meta_ok || !*has_meta) {
        return 0;
    }

    time_t now = time(NULL);
    if (cache_is_fresh(meta, now)) {
        log_msg(cfg, "INFO", "Кэш-попадание: %s", cache_path);
        send_file(client_fd, cache_path);
        return 1;
    }

    return 0;
}

static void update_meta_from_response(cache_meta_t *meta, const http_response_info_t *info) {
    time_t now = time(NULL);
    if (info->has_max_age) {
        meta->expires = now + info->max_age;
    } else if (info->has_expires) {
        meta->expires = info->expires;
    } else if (meta->expires == 0) {
        meta->expires = 0;
    }

    meta->must_revalidate = info->must_revalidate || info->no_cache;

    if (info->has_last_modified) {
        copy_str(meta->last_modified, sizeof(meta->last_modified), info->last_modified);
    }
    if (info->has_etag) {
        copy_str(meta->etag, sizeof(meta->etag), info->etag);
    }
}

static int forward_and_cache(int server_fd, int client_fd, const proxy_config_t *cfg,
                             const char *cache_path, const char *meta_path,
                             const http_response_info_t *info, const char *header_buf,
                             size_t total_len, size_t header_len, int allow_cache) {
    FILE *cache_file = NULL;
    char tmp_path[PATH_MAX];

    if (allow_cache) {
        char suffix[64];
        int sn = snprintf(suffix, sizeof(suffix), ".tmp.%ld.%lu", (long)getpid(), (unsigned long)pthread_self());
        if (sn < 0 || (size_t)sn >= sizeof(suffix)) {
            allow_cache = 0;
        } else if (strlen(cache_path) + (size_t)sn + 1 > sizeof(tmp_path)) {
            allow_cache = 0;
        } else {
            size_t base_len = strlen(cache_path);
            memcpy(tmp_path, cache_path, base_len);
            memcpy(tmp_path + base_len, suffix, (size_t)sn);
            tmp_path[base_len + (size_t)sn] = '\0';
        }

        if (allow_cache) {
            cache_file = fopen(tmp_path, "wb");
            if (!cache_file) {
                log_msg(cfg, "ERROR", "Не удалось открыть файл кэша: %s", tmp_path);
                allow_cache = 0;
            }
        }
    }

    if (send_all(client_fd, header_buf, header_len) != 0) {
        if (cache_file) {
            fclose(cache_file);
            unlink(tmp_path);
        }
        return -1;
    }

    if (cache_file) {
        fwrite(header_buf, 1, header_len, cache_file);
    }

    size_t body_len = total_len > header_len ? total_len - header_len : 0;
    if (body_len > 0) {
        if (send_all(client_fd, header_buf + header_len, body_len) != 0) {
            if (cache_file) {
                fclose(cache_file);
                unlink(tmp_path);
            }
            return -1;
        }
        if (cache_file) {
            fwrite(header_buf + header_len, 1, body_len, cache_file);
        }
    }

    char buf[IO_BUF_SIZE];
    ssize_t n;
    while ((n = recv(server_fd, buf, sizeof(buf), 0)) > 0) {
        if (send_all(client_fd, buf, (size_t)n) != 0) {
            if (cache_file) {
                fclose(cache_file);
                unlink(tmp_path);
            }
            return -1;
        }
        if (cache_file) {
            fwrite(buf, 1, (size_t)n, cache_file);
        }
    }

    if (cache_file) {
        fclose(cache_file);
        if (rename(tmp_path, cache_path) != 0) {
            log_msg(cfg, "ERROR", "Не удалось сохранить кэш: %s", cache_path);
            unlink(tmp_path);
        } else {
            cache_meta_t meta;
            memset(&meta, 0, sizeof(meta));
            meta.stored_at = time(NULL);
            update_meta_from_response(&meta, info);

            pthread_mutex_lock(&cache_mutex);
            cache_meta_write(meta_path, &meta);
            pthread_mutex_unlock(&cache_mutex);
        }
    }

    return 0;
}

static int handle_get_request(int client_fd, const http_request_t *req, const proxy_config_t *cfg) {
    char url[4096];
    snprintf(url, sizeof(url), "http://%s:%d%s", req->host, req->port, req->path);

    char cache_path[PATH_MAX];
    char meta_path[PATH_MAX];
    char key[32];
    int cache_ready = build_cache_paths(cfg, url, cache_path, sizeof(cache_path), meta_path, sizeof(meta_path), key, sizeof(key)) == 0;

    cache_meta_t meta;
    int has_meta = 0;

    if (cache_ready) {
        if (try_serve_cache(cfg, cache_path, meta_path, &meta, &has_meta, client_fd)) {
            return 0;
        }
    } else {
        log_msg(cfg, "ERROR", "Слишком длинный путь к кэшу, кэш отключён для запроса");
    }

    log_msg(cfg, "INFO", "Кэш-промах: %s", url);

    char cond_headers[512];
    cond_headers[0] = '\0';
    if (cache_ready && has_meta) {
        if (meta.last_modified[0]) {
            snprintf(cond_headers + strlen(cond_headers),
                     sizeof(cond_headers) - strlen(cond_headers),
                     "If-Modified-Since: %s\r\n", meta.last_modified);
        }
        if (meta.etag[0]) {
            snprintf(cond_headers + strlen(cond_headers),
                     sizeof(cond_headers) - strlen(cond_headers),
                     "If-None-Match: %s\r\n", meta.etag);
        }
    }

    char *forward_req = NULL;
    size_t forward_len = 0;
    if (build_forward_request(req, cond_headers, &forward_req, &forward_len) != 0) {
        send_simple_response(client_fd, 500, "Internal Server Error", "Ошибка формирования запроса\n");
        return -1;
    }

    char err[256];
    int server_fd = connect_to_host(req->host, req->port, err, sizeof(err));
    if (server_fd < 0) {
        log_msg(cfg, "ERROR", "%s", err);
        send_simple_response(client_fd, 502, "Bad Gateway", "Не удалось подключиться к серверу\n");
        free(forward_req);
        return -1;
    }

    if (send_all(server_fd, forward_req, forward_len) != 0) {
        log_msg(cfg, "ERROR", "Ошибка отправки запроса на сервер");
        send_simple_response(client_fd, 502, "Bad Gateway", "Ошибка отправки запроса\n");
        free(forward_req);
        close(server_fd);
        return -1;
    }
    free(forward_req);

    char *resp_buf = NULL;
    size_t resp_len = 0;
    size_t header_len = 0;
    if (recv_response_header(server_fd, &resp_buf, &resp_len, &header_len, err, sizeof(err)) != 0) {
        log_msg(cfg, "ERROR", "Ошибка чтения ответа: %s", err);
        send_simple_response(client_fd, 502, "Bad Gateway", "Ошибка чтения ответа\n");
        close(server_fd);
        return -1;
    }

    http_response_info_t info;
    if (parse_response_info(resp_buf, header_len, &info) != 0) {
        free(resp_buf);
        close(server_fd);
        send_simple_response(client_fd, 502, "Bad Gateway", "Некорректный ответ сервера\n");
        return -1;
    }

    if (info.status_code == 304 && cache_ready && has_meta) {
        log_msg(cfg, "INFO", "Кэш обновлён (304 Not Modified): %s", url);
        update_meta_from_response(&meta, &info);
        pthread_mutex_lock(&cache_mutex);
        cache_meta_write(meta_path, &meta);
        pthread_mutex_unlock(&cache_mutex);

        send_file(client_fd, cache_path);
        free(resp_buf);
        close(server_fd);
        return 0;
    }

    int allow_cache = (cache_ready && info.status_code == 200 && !info.no_store);
    if (!allow_cache) {
        log_msg(cfg, "DEBUG", "Ответ не кэшируется (код=%d)", info.status_code);
    }

    forward_and_cache(server_fd, client_fd, cfg, cache_path, meta_path, &info,
                      resp_buf, resp_len, header_len, allow_cache);

    free(resp_buf);
    close(server_fd);
    return 0;
}

static int handle_post_request(int client_fd, const http_request_t *req, const proxy_config_t *cfg) {
    char *forward_req = NULL;
    size_t forward_len = 0;
    if (build_forward_request(req, NULL, &forward_req, &forward_len) != 0) {
        send_simple_response(client_fd, 500, "Internal Server Error", "Ошибка формирования запроса\n");
        return -1;
    }

    char err[256];
    int server_fd = connect_to_host(req->host, req->port, err, sizeof(err));
    if (server_fd < 0) {
        log_msg(cfg, "ERROR", "%s", err);
        send_simple_response(client_fd, 502, "Bad Gateway", "Не удалось подключиться к серверу\n");
        free(forward_req);
        return -1;
    }

    if (send_all(server_fd, forward_req, forward_len) != 0) {
        log_msg(cfg, "ERROR", "Ошибка отправки запроса POST");
        send_simple_response(client_fd, 502, "Bad Gateway", "Ошибка отправки запроса\n");
        free(forward_req);
        close(server_fd);
        return -1;
    }
    free(forward_req);

    if (req->body_len > 0) {
        if (send_all(server_fd, req->body, req->body_len) != 0) {
            log_msg(cfg, "ERROR", "Ошибка отправки тела POST");
            send_simple_response(client_fd, 502, "Bad Gateway", "Ошибка отправки тела запроса\n");
            close(server_fd);
            return -1;
        }
    }

    char *resp_buf = NULL;
    size_t resp_len = 0;
    size_t header_len = 0;
    if (recv_response_header(server_fd, &resp_buf, &resp_len, &header_len, err, sizeof(err)) != 0) {
        log_msg(cfg, "ERROR", "Ошибка чтения ответа: %s", err);
        send_simple_response(client_fd, 502, "Bad Gateway", "Ошибка чтения ответа\n");
        close(server_fd);
        return -1;
    }

    if (send_all(client_fd, resp_buf, header_len) != 0) {
        free(resp_buf);
        close(server_fd);
        return -1;
    }

    size_t body_len = resp_len > header_len ? resp_len - header_len : 0;
    if (body_len > 0) {
        if (send_all(client_fd, resp_buf + header_len, body_len) != 0) {
            free(resp_buf);
            close(server_fd);
            return -1;
        }
    }

    char buf[IO_BUF_SIZE];
    ssize_t n;
    while ((n = recv(server_fd, buf, sizeof(buf), 0)) > 0) {
        if (send_all(client_fd, buf, (size_t)n) != 0) {
            break;
        }
    }

    free(resp_buf);
    close(server_fd);
    return 0;
}

static void *client_thread(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int fd = ctx->client_fd;
    proxy_config_t *cfg = ctx->config;

    http_request_t req;
    char err[256];
    if (read_request(fd, &req, err, sizeof(err)) != 0) {
        log_msg(cfg, "ERROR", "Ошибка запроса: %s", err);
        send_simple_response(fd, 400, "Bad Request", "Некорректный запрос\n");
        close(fd);
        free(ctx);
        return NULL;
    }

    if (strcasecmp(req.method, "GET") == 0) {
        handle_get_request(fd, &req, cfg);
    } else if (strcasecmp(req.method, "POST") == 0) {
        log_msg(cfg, "INFO", "POST запрос: %s%s", req.host, req.path);
        handle_post_request(fd, &req, cfg);
    } else {
        send_simple_response(fd, 501, "Not Implemented", "Поддерживаются только GET и POST\n");
    }

    if (req.body) {
        free(req.body);
    }

    close(fd);
    free(ctx);
    return NULL;
}

int main(int argc, char **argv) {
    int port = 0;
    proxy_config_t cfg;
    cfg.debug = 0;
    snprintf(cfg.cache_dir, sizeof(cfg.cache_dir), "./cache");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-cache_dir") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            snprintf(cfg.cache_dir, sizeof(cfg.cache_dir), "%s", argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            cfg.debug = 1;
        } else if (port == 0) {
            port = atoi(argv[i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        usage(argv[0]);
        return 1;
    }

    if (ensure_cache_dir(cfg.cache_dir) != 0) {
        fprintf(stderr, "Не удалось создать каталог кэша: %s\n", cfg.cache_dir);
        return 1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 64) != 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    log_msg(&cfg, "INFO", "HTTP Proxy запущен на порту %d", port);
    log_msg(&cfg, "INFO", "Каталог кэша: %s", cfg.cache_dir);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        client_ctx_t *ctx = (client_ctx_t *)malloc(sizeof(client_ctx_t));
        if (!ctx) {
            close(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;
        ctx->config = &cfg;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
            close(client_fd);
            free(ctx);
            continue;
        }
        pthread_detach(tid);
    }

    close(listen_fd);
    return 0;
}
