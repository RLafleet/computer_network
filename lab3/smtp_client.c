#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "smtp_utils.h"
#include "mime_utils.h"
#include "tls_utils.h"

void print_usage(const char* program_name) {
    fprintf(stderr, "Использование: %s [опции]\n", program_name);
    fprintf(stderr, "\nОсновные опции:\n");
    fprintf(stderr, "  -s, --server СЕРВЕР      Адрес SMTP сервера\n");
    fprintf(stderr, "  -p, --port ПОРТ          Порт SMTP сервера (25, 587)\n");
    fprintf(stderr, "  -f, --from EMAIL         Адрес отправителя\n");
    fprintf(stderr, "  -t, --to EMAIL           Адрес получателя\n");
    fprintf(stderr, "  -j, --subject ТЕМА       Тема письма\n");
    fprintf(stderr, "  -b, --body ТЕКСТ         Текст письма или путь к файлу\n");
    fprintf(stderr, "  -a, --attachment ФАЙЛ    Вложение (можно использовать несколько раз)\n");
    fprintf(stderr, "  -l, --login ИМЯ          Имя пользователя для аутентификации\n");
    fprintf(stderr, "  -w, --password ПАРОЛЬ    Пароль для аутентификации\n");
    fprintf(stderr, "      --tls, --ssl         Использовать TLS/SSL шифрование\n");
    fprintf(stderr, "  -v, --verbose            Подробный вывод\n");
    fprintf(stderr, "  -h, --help               Показать эту справку\n");
    fprintf(stderr, "\nПримеры:\n");
    fprintf(stderr, "  # Простое письмо\n");
    fprintf(stderr, "  %s -s smtp.mail.ru -p 25 -f sender@mail.ru -t recipient@mail.ru -j \"Тест\" -b \"Привет\"\n", program_name);
    fprintf(stderr, "\n  # Защищенное письмо с аутентификацией\n");
    fprintf(stderr, "  %s -s smtp.gmail.com -p 587 --tls -l user@gmail.com -w \"пароль\" -f user@gmail.com -t recipient@mail.ru -j \"TLS тест\" -b \"Защищенное сообщение\"\n", program_name);
    fprintf(stderr, "\n  # Письмо с вложениями\n");
    fprintf(stderr, "  %s -s smtp.yandex.ru -p 587 --tls -l user@yandex.ru -w \"пароль\" -f user@yandex.ru -t recipient@mail.ru -j \"Файлы\" -b \"Смотрите вложения\" -a document.pdf -a photo.jpg\n", program_name);
}

void init_config(smtp_config_t* config) {
    config->server = NULL;
    config->port = 25;
    config->sender = NULL;
    config->recipient = NULL;
    config->subject = NULL;
    config->body = NULL;
    config->body_file = NULL;
    config->attachment_count = 0;
    config->login = NULL;
    config->password = NULL;
    config->use_tls = 0;
    config->verbose = 0;
    
    for (int i = 0; i < MAX_ATTACHMENTS; i++) {
        config->attachments[i] = NULL;
    }
}

void free_config(smtp_config_t* config) {
    if (config->server) free(config->server);
    if (config->sender) free(config->sender);
    if (config->recipient) free(config->recipient);
    if (config->subject) free(config->subject);
    if (config->body) free(config->body);
    if (config->body_file) free(config->body_file);
    if (config->login) free(config->login);
    if (config->password) free(config->password);
    
    for (int i = 0; i < config->attachment_count; i++) {
        if (config->attachments[i]) free(config->attachments[i]);
    }
}

int parse_arguments(int argc, char *argv[], smtp_config_t* config) {
    static struct option long_options[] = {
        {"server", required_argument, 0, 's'},
        {"port", required_argument, 0, 'p'},
        {"from", required_argument, 0, 'f'},
        {"to", required_argument, 0, 't'},
        {"subject", required_argument, 0, 'j'},
        {"body", required_argument, 0, 'b'},
        {"attachment", required_argument, 0, 'a'},
        {"login", required_argument, 0, 'l'},
        {"password", required_argument, 0, 'w'},
        {"tls", no_argument, 0, 1},
        {"ssl", no_argument, 0, 1},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "s:p:f:t:j:b:a:l:w:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                config->server = strdup(optarg);
                break;
            case 'p':
                config->port = atoi(optarg);
                break;
            case 'f':
                config->sender = strdup(optarg);
                break;
            case 't':
                config->recipient = strdup(optarg);
                break;
            case 'j':
                config->subject = strdup(optarg);
                break;
            case 'b':
                if (access(optarg, F_OK) == 0) {
                    config->body_file = strdup(optarg);
                } else {
                    config->body = strdup(optarg);
                }
                break;
            case 'a':
                if (config->attachment_count < MAX_ATTACHMENTS) {
                    config->attachments[config->attachment_count] = strdup(optarg);
                    config->attachment_count++;
                } else {
                    fprintf(stderr, "Предупреждение: Достигнуто максимальное количество вложений (%d)\n", MAX_ATTACHMENTS);
                }
                break;
            case 'l':
                config->login = strdup(optarg);
                break;
            case 'w':
                config->password = strdup(optarg);
                break;
            case 1:
                config->use_tls = 1;
                break;
            case 'v':
                config->verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    if (!config->server || !config->sender || !config->recipient || 
        (!config->body && !config->body_file && config->attachment_count == 0)) {
        fprintf(stderr, "Ошибка: Отсутствуют обязательные аргументы\n");
        print_usage(argv[0]);
        return -1;
    }
    
    if (config->body_file) {
        FILE* file = fopen(config->body_file, "r");
        if (!file) {
            perror("Ошибка открытия файла с текстом письма");
            return -1;
        }
        
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        config->body = malloc(file_size + 1);
        if (!config->body) {
            fclose(file);
            fprintf(stderr, "Ошибка выделения памяти для файла с текстом письма\n");
            return -1;
        }
        
        size_t result = fread(config->body, 1, file_size, file);
        (void)result;
        config->body[file_size] = '\0';
        fclose(file);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    smtp_config_t config;
    smtp_connection_t conn;
    
    init_config(&config);
    
    if (parse_arguments(argc, argv, &config) != 0) {
        free_config(&config);
        return EXIT_FAILURE;
    }
    
    if (config.use_tls) {
        if (init_openssl() != 0) {
            fprintf(stderr, "Ошибка инициализации OpenSSL\n");
            free_config(&config);
            return EXIT_FAILURE;
        }
    }
    
    if (connect_to_server(&config, &conn) != 0) {
        if (config.use_tls) cleanup_openssl();
        free_config(&config);
        return EXIT_FAILURE;
    }
    
    int result = handle_smtp_dialog(&conn, &config);
    
    if (conn.sockfd >= 0) {
        if (conn.use_tls) {
#ifdef USE_OPENSSL
            if (conn.ssl) {
                SSL_shutdown(conn.ssl);
                SSL_free(conn.ssl);
            }
            if (conn.ctx) {
                SSL_CTX_free(conn.ctx);
            }
#endif
        }
        close(conn.sockfd);
    }
    
    if (config.use_tls) {
        cleanup_openssl();
    }
    
    free_config(&config);
    
    if (result == 0) {
        printf("Письмо успешно отправлено!\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Не удалось отправить письмо\n");
        return EXIT_FAILURE;
    }
}