#include "smtp_utils.h"
#include "mime_utils.h"
#include "tls_utils.h"
#include <string.h>

int connect_to_server(smtp_config_t* config, smtp_connection_t* conn) {
    struct sockaddr_in server_addr;
    struct hostent *host;
    
    conn->sockfd = -1;
    conn->use_tls = config->use_tls;
    conn->verbose = config->verbose;
    
#ifdef USE_OPENSSL
    conn->ssl = NULL;
    conn->ctx = NULL;
#endif
    
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd == -1) {
        perror("socket() не удался");
        return -1;
    }
    
    host = gethostbyname(config->server);
    if (host == NULL) {
        fprintf(stderr, "Ошибка: Не удалось разрешить имя хоста %s\n", config->server);
        close(conn->sockfd);
        conn->sockfd = -1;
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (connect(conn->sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect() не удался");
        close(conn->sockfd);
        conn->sockfd = -1;
        return -1;
    }
    
    if (conn->verbose) {
        printf("Подключено к %s:%d\n", config->server, config->port);
    }
    
    if (config->use_tls && config->port == 587) {
#ifdef USE_OPENSSL
        if (starttls_handshake(conn) != 0) {
            fprintf(stderr, "Ошибка: Не удалось выполнить STARTTLS handshake\n");
            close(conn->sockfd);
            conn->sockfd = -1;
            return -1;
        }
#else
        fprintf(stderr, "Ошибка: Поддержка TLS не скомпилирована\n");
        close(conn->sockfd);
        conn->sockfd = -1;
        return -1;
#endif
    }
    
    return 0;
}

int read_response(smtp_connection_t* conn, char* buffer, size_t buffer_size) {
    ssize_t bytes_received;
    int offset = 0;
    int status_code = 0;
    char code_str[4] = {'\0', '\0', '\0', '\0'};
    
    memset(buffer, 0, buffer_size);
    
    while (offset < (int)(buffer_size - 1)) {
        if (conn->use_tls) {
#ifdef USE_OPENSSL
            if (conn->ssl) {
                bytes_received = SSL_read(conn->ssl, buffer + offset, 1);
            } else {
                bytes_received = read(conn->sockfd, buffer + offset, 1);
            }
#else
            bytes_received = read(conn->sockfd, buffer + offset, 1);
#endif
        } else {
            bytes_received = read(conn->sockfd, buffer + offset, 1);
        }
        
        if (bytes_received <= 0) {
            if (bytes_received == -1) {
                if (conn->use_tls) {
#ifdef USE_OPENSSL
                    if (conn->ssl) {
                        int ssl_error = SSL_get_error(conn->ssl, bytes_received);
                        switch (ssl_error) {
                            case SSL_ERROR_ZERO_RETURN:
                                fprintf(stderr, "SSL соединение закрыто сервером\n");
                                break;
                            case SSL_ERROR_WANT_READ:
                            case SSL_ERROR_WANT_WRITE:
                                fprintf(stderr, "SSL операция не готова\n");
                                break;
                            case SSL_ERROR_SYSCALL:
                                perror("SSL read() системный вызов не удался");
                                break;
                            case SSL_ERROR_SSL:
                                fprintf(stderr, "Ошибка SSL протокола\n");
                                break;
                            default:
                                fprintf(stderr, "Ошибка чтения SSL: %d\n", ssl_error);
                                break;
                        }
                    } else {
                        perror("read() не удался");
                    }
#else
                    perror("read() не удался");
#endif
                } else {
                    perror("read() не удался");
                }
            } else {
                fprintf(stderr, "Соединение закрыто сервером\n");
            }
            return -1;
        }
        
        offset += bytes_received;
        
        if (offset >= 2 && buffer[offset-2] == '\r' && buffer[offset-1] == '\n') {
            break;
        }
    }
    
    buffer[offset] = '\0';
    
    if (offset >= 3) {
        code_str[0] = buffer[0];
        code_str[1] = buffer[1];
        code_str[2] = buffer[2];
        status_code = atoi(code_str);
    }
    
    if (conn->verbose) {
        printf("Сервер: %s", buffer);
    }
    
    if (offset >= 4 && buffer[3] == '-') {
        while (1) {
            int line_offset = 0;
            char line_buffer[BUFFER_SIZE];
            
            while (line_offset < (int)(sizeof(line_buffer) - 1)) {
                if (conn->use_tls) {
#ifdef USE_OPENSSL
                    if (conn->ssl) {
                        bytes_received = SSL_read(conn->ssl, line_buffer + line_offset, 1);
                    } else {
                        bytes_received = read(conn->sockfd, line_buffer + line_offset, 1);
                    }
#else
                    bytes_received = read(conn->sockfd, line_buffer + line_offset, 1);
#endif
                } else {
                    bytes_received = read(conn->sockfd, line_buffer + line_offset, 1);
                }
                
                if (bytes_received <= 0) {
                    if (bytes_received == -1) {
                        perror("read() не удался");
                    } else {
                        fprintf(stderr, "Соединение закрыто сервером\n");
                    }
                    return -1;
                }
                
                line_offset += bytes_received;
                
                if (line_offset >= 2 && line_buffer[line_offset-2] == '\r' && line_buffer[line_offset-1] == '\n') {
                    break;
                }
            }
            
            line_buffer[line_offset] = '\0';
            
            if (conn->verbose) {
                printf("Сервер: %s", line_buffer);
            }
            
            if (line_offset >= 4 && 
                line_buffer[0] == code_str[0] && 
                line_buffer[1] == code_str[1] && 
                line_buffer[2] == code_str[2] && 
                line_buffer[3] != '-') {
                break;
            }
            
            if (offset + line_offset >= (int)(buffer_size - 1)) {
                break;
            }
            
            strncat(buffer, line_buffer, buffer_size - offset - 1);
            offset += line_offset;
        }
    }
    
    return status_code;
}

int send_command(smtp_connection_t* conn, const char* command) {
    char full_command[BUFFER_SIZE];
    ssize_t bytes_sent;
    
    snprintf(full_command, sizeof(full_command), "%s\r\n", command);
    
    if (conn->verbose) {
        printf("Клиент: %s\n", command);
    }
    
    if (conn->use_tls) {
#ifdef USE_OPENSSL
        if (conn->ssl) {
            bytes_sent = SSL_write(conn->ssl, full_command, strlen(full_command));
        } else {
            bytes_sent = write(conn->sockfd, full_command, strlen(full_command));
        }
#else
        bytes_sent = write(conn->sockfd, full_command, strlen(full_command));
#endif
    } else {
        bytes_sent = write(conn->sockfd, full_command, strlen(full_command));
    }
    
    if (bytes_sent == -1) {
        if (conn->use_tls) {
#ifdef USE_OPENSSL
            if (conn->ssl) {
                int ssl_error = SSL_get_error(conn->ssl, bytes_sent);
                switch (ssl_error) {
                    case SSL_ERROR_ZERO_RETURN:
                        fprintf(stderr, "SSL соединение закрыто сервером\n");
                        break;
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                        fprintf(stderr, "SSL операция не готова\n");
                        break;
                    case SSL_ERROR_SYSCALL:
                        perror("SSL write() системный вызов не удался");
                        break;
                    case SSL_ERROR_SSL:
                        fprintf(stderr, "Ошибка SSL протокола\n");
                        break;
                    default:
                        fprintf(stderr, "Ошибка записи SSL: %d\n", ssl_error);
                        break;
                }
            } else {
                perror("write() не удался");
            }
#else
            perror("write() не удался");
#endif
        } else {
            perror("write() не удался");
        }
        return -1;
    }
    
    return 0;
}

int handle_smtp_dialog(smtp_connection_t* conn, smtp_config_t* config) {
    char buffer[BUFFER_SIZE];
    int status_code;
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 220) {
        fprintf(stderr, "Ошибка: Ожидался код состояния 220, получен %d\n", status_code);
        return -1;
    }
    
    char ehlo_cmd[BUFFER_SIZE];
    snprintf(ehlo_cmd, sizeof(ehlo_cmd), "EHLO client.example.com");
    if (send_command(conn, ehlo_cmd) == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 250) {
        fprintf(stderr, "Ошибка: Команда EHLO не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    if (config->login && config->password) {
        if (authenticate_user(conn, config->login, config->password) != 0) {
            fprintf(stderr, "Ошибка: Аутентификация не удалась\n");
            return -1;
        }
    }
    
    char mail_from_cmd[BUFFER_SIZE];
    snprintf(mail_from_cmd, sizeof(mail_from_cmd), "MAIL FROM:<%s>", config->sender);
    if (send_command(conn, mail_from_cmd) == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 250) {
        fprintf(stderr, "Ошибка: Команда MAIL FROM не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    char rcpt_to_cmd[BUFFER_SIZE];
    snprintf(rcpt_to_cmd, sizeof(rcpt_to_cmd), "RCPT TO:<%s>", config->recipient);
    if (send_command(conn, rcpt_to_cmd) == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 250) {
        fprintf(stderr, "Ошибка: Команда RCPT TO не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    if (send_command(conn, "DATA") == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 354) {
        fprintf(stderr, "Ошибка: Команда DATA не удалась с кодом состояния %d, ожидался 354\n", status_code);
        fprintf(stderr, "Возможно сервер не следует стандартному протоколу SMTP\n");
        return -1;
    }
    
    if (config->attachment_count > 0) {
        char* boundary = generate_boundary();
        char* mime_message = create_mime_message(
            config->sender,
            config->recipient,
            config->subject ? config->subject : "",
            config->body ? config->body : "",
            config->attachments,
            config->attachment_count,
            boundary
        );
        
        if (mime_message) {
            if (conn->verbose) {
                printf("Клиент: Отправка MIME содержимого письма...\n");
            }
            
            ssize_t bytes_sent;
            if (conn->use_tls) {
#ifdef USE_OPENSSL
                if (conn->ssl) {
                    bytes_sent = SSL_write(conn->ssl, mime_message, strlen(mime_message));
                } else {
                    bytes_sent = write(conn->sockfd, mime_message, strlen(mime_message));
                }
#else
                bytes_sent = write(conn->sockfd, mime_message, strlen(mime_message));
#endif
            } else {
                bytes_sent = write(conn->sockfd, mime_message, strlen(mime_message));
            }
            
            if (bytes_sent == -1) {
                perror("write() не удался");
                free(mime_message);
                free(boundary);
                return -1;
            }
            
            free(mime_message);
            free(boundary);
        } else {
            fprintf(stderr, "Ошибка: Не удалось создать MIME сообщение\n");
            return -1;
        }
    } else {
        char email_content[MAX_EMAIL_SIZE];
        snprintf(email_content, sizeof(email_content), 
                 "From: <%s>\r\n"
                 "To: <%s>\r\n"
                 "Subject: %s\r\n"
                 "\r\n"
                 "%s\r\n"
                 ".\r\n",
                 config->sender, config->recipient, 
                 config->subject ? config->subject : "", 
                 config->body ? config->body : "");
        
        if (conn->verbose) {
            printf("Клиент: Отправка содержимого письма...\n");
        }
        
        ssize_t bytes_sent;
        if (conn->use_tls) {
#ifdef USE_OPENSSL
            if (conn->ssl) {
                bytes_sent = SSL_write(conn->ssl, email_content, strlen(email_content));
            } else {
                bytes_sent = write(conn->sockfd, email_content, strlen(email_content));
            }
#else
            bytes_sent = write(conn->sockfd, email_content, strlen(email_content));
#endif
        } else {
            bytes_sent = write(conn->sockfd, email_content, strlen(email_content));
        }
        
        if (bytes_sent == -1) {
            perror("write() не удался");
            return -1;
        }
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 250) {
        fprintf(stderr, "Ошибка: Передача письма не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    if (send_command(conn, "QUIT") == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 221) {
        fprintf(stderr, "Предупреждение: Команда QUIT не удалась с кодом состояния %d\n", status_code);
    }
    
    return 0;
}