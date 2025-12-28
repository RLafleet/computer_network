#include "tls_utils.h"
#include "smtp_utils.h"
#include <ctype.h>

#ifdef USE_OPENSSL

int init_openssl(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

int cleanup_openssl(void) {
    EVP_cleanup();
    return 0;
}

int connect_with_starttls(smtp_connection_t* conn, const char* server, int port) {
    return 0;
}

int starttls_handshake(smtp_connection_t* conn) {
    char buffer[BUFFER_SIZE];
    int status_code;
    
    if (send_command(conn, "STARTTLS") == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 220) {
        fprintf(stderr, "Ошибка: Команда STARTTLS не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    const SSL_METHOD* method = TLS_client_method();
    conn->ctx = SSL_CTX_new(method);
    if (!conn->ctx) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    conn->ssl = SSL_new(conn->ctx);
    if (!conn->ssl) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(conn->ctx);
        conn->ctx = NULL;
        return -1;
    }
    
    if (SSL_set_fd(conn->ssl, conn->sockfd) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(conn->ssl);
        SSL_CTX_free(conn->ctx);
        conn->ssl = NULL;
        conn->ctx = NULL;
        return -1;
    }
    
    if (SSL_connect(conn->ssl) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(conn->ssl);
        SSL_CTX_free(conn->ctx);
        conn->ssl = NULL;
        conn->ctx = NULL;
        return -1;
    }
    
    if (conn->verbose) {
        printf("SSL/TLS соединение установлено\n");
    }
    
    return 0;
}

static char* base64_encode_auth(const unsigned char* data, size_t input_length) {
    static const char encoding_table[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };
    
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(output_length + 1);
    if (!encoded_data) return NULL;
    
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;
        
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }
    
    for (size_t i = 0; i < (3 - (input_length % 3)) % 3; i++) {
        encoded_data[output_length - 1 - i] = '=';
    }
    
    encoded_data[output_length] = '\0';
    return encoded_data;
}

int authenticate_user_login(smtp_connection_t* conn, const char* username, const char* password) {
    char buffer[BUFFER_SIZE];
    int status_code;
    
    if (send_command(conn, "AUTH LOGIN") == -1) {
        return -1;
    }
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 334) {
        fprintf(stderr, "Ошибка: Команда AUTH LOGIN не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    char* encoded_username = base64_encode_auth((const unsigned char*)username, strlen(username));
    if (!encoded_username) {
        fprintf(stderr, "Ошибка: Не удалось закодировать имя пользователя\n");
        return -1;
    }
    
    if (send_command(conn, encoded_username) == -1) {
        free(encoded_username);
        return -1;
    }
    free(encoded_username);
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 334) {
        fprintf(stderr, "Ошибка: Имя пользователя отклонено с кодом состояния %d\n", status_code);
        return -1;
    }
    
    char* encoded_password = base64_encode_auth((const unsigned char*)password, strlen(password));
    if (!encoded_password) {
        fprintf(stderr, "Ошибка: Не удалось закодировать пароль\n");
        return -1;
    }
    
    if (send_command(conn, encoded_password) == -1) {
        free(encoded_password);
        return -1;
    }
    free(encoded_password);
    
    status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 235) {
        fprintf(stderr, "Ошибка: Аутентификация не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    return 0;
}

int authenticate_user_plain(smtp_connection_t* conn, const char* username, const char* password) {
    size_t auth_len = 1 + strlen(username) + 1 + strlen(password);
    unsigned char* auth_string = malloc(auth_len);
    if (!auth_string) {
        return -1;
    }
    
    auth_string[0] = '\0';
    memcpy(auth_string + 1, username, strlen(username));
    auth_string[1 + strlen(username)] = '\0';
    memcpy(auth_string + 1 + strlen(username) + 1, password, strlen(password));
    
    char* encoded_auth = base64_encode_auth(auth_string, auth_len);
    free(auth_string);
    
    if (!encoded_auth) {
        return -1;
    }
    
    char auth_cmd[BUFFER_SIZE];
    snprintf(auth_cmd, sizeof(auth_cmd), "AUTH PLAIN %s", encoded_auth);
    free(encoded_auth);
    
    if (send_command(conn, auth_cmd) == -1) {
        return -1;
    }
    
    char buffer[BUFFER_SIZE];
    int status_code = read_response(conn, buffer, sizeof(buffer));
    if (status_code != 235) {
        fprintf(stderr, "Ошибка: AUTH PLAIN не удалась с кодом состояния %d\n", status_code);
        return -1;
    }
    
    return 0;
}

int authenticate_user(smtp_connection_t* conn, const char* username, const char* password) {
    if (authenticate_user_plain(conn, username, password) == 0) {
        if (conn->verbose) {
            printf("Аутентификация успешна (AUTH PLAIN)\n");
        }
        return 0;
    }
    
    if (authenticate_user_login(conn, username, password) == 0) {
        if (conn->verbose) {
            printf("Аутентификация успешна (AUTH LOGIN)\n");
        }
        return 0;
    }
    
    fprintf(stderr, "Ошибка: Оба метода AUTH PLAIN и AUTH LOGIN не удались\n");
    return -1;
}

#else

int init_openssl(void) { return 0; }
int cleanup_openssl(void) { return 0; }
int connect_with_starttls(smtp_connection_t* conn, const char* server, int port) { 
    (void)conn; (void)server; (void)port;
    return -1; 
}
int starttls_handshake(smtp_connection_t* conn) { 
    (void)conn;
    return -1; 
}
int authenticate_user(smtp_connection_t* conn, const char* username, const char* password) { 
    (void)conn; (void)username; (void)password;
    return -1; 
}

#endif