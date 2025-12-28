#include "mime_utils.h"
#include <time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <strings.h>

char* generate_boundary(void) {
    char* boundary = malloc(MAX_BOUNDARY_LENGTH);
    if (!boundary) return NULL;
    
    srand(time(NULL));
    snprintf(boundary, MAX_BOUNDARY_LENGTH, "----MIMEBoundary%08x", rand());
    
    return boundary;
}

char* get_content_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcasecmp(ext, ".gif") == 0) {
        return "image/gif";
    } else if (strcasecmp(ext, ".pdf") == 0) {
        return "application/pdf";
    } else if (strcasecmp(ext, ".txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(ext, ".html") == 0) {
        return "text/html";
    } else if (strcasecmp(ext, ".xml") == 0) {
        return "application/xml";
    } else if (strcasecmp(ext, ".zip") == 0) {
        return "application/zip";
    } else if (strcasecmp(ext, ".doc") == 0) {
        return "application/msword";
    } else if (strcasecmp(ext, ".docx") == 0) {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    } else {
        return "application/octet-stream";
    }
}

char* base64_encode(const unsigned char* data, size_t input_length, size_t* output_length) {
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
    
    *output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(*output_length + 1);
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
        encoded_data[*output_length - 1 - i] = '=';
    }
    
    encoded_data[*output_length] = '\0';
    return encoded_data;
}

char* encode_file_base64(const char* filepath, size_t* encoded_length) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("Ошибка открытия файла для кодирования base64");
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    unsigned char* file_content = malloc(file_size);
    if (!file_content) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(file_content, 1, file_size, file);
    fclose(file);
    
    if ((long)bytes_read != file_size) {
        free(file_content);
        return NULL;
    }
    
    char* encoded = base64_encode(file_content, file_size, encoded_length);
    free(file_content);
    
    return encoded;
}

void print_progress(size_t current, size_t total) {
    if (total == 0) return;
    
    int percentage = (int)((current * 100) / total);
    printf("\rПрогресс: [%-20s] %d%%", "", percentage);
    
    int pos = percentage / 5;
    for (int i = 0; i < pos; i++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b--------------------");
        for (int j = 0; j < i; j++) printf("#");
        fflush(stdout);
    }
    
    if (current == total) {
        printf("\n");
    }
}

char* create_mime_message(const char* sender, const char* recipient, 
                         const char* subject, const char* body,
                         char** attachments, int attachment_count,
                         const char* boundary) {
    if (!boundary) return NULL;
    
    size_t total_size = 1024;
    total_size += strlen(sender) + strlen(recipient) + strlen(subject) + strlen(body);
    
    for (int i = 0; i < attachment_count; i++) {
        struct stat st;
        if (stat(attachments[i], &st) == 0) {
            total_size += st.st_size * 2;
            total_size += 512;
        }
    }
    
    char* mime_message = malloc(total_size);
    if (!mime_message) return NULL;
    
    size_t offset = 0;
    offset += snprintf(mime_message + offset, total_size - offset,
                       "From: <%s>\r\n"
                       "To: <%s>\r\n"
                       "Subject: %s\r\n"
                       "MIME-Version: 1.0\r\n"
                       "Content-Type: multipart/mixed; boundary=\"%s\"\r\n"
                       "\r\n"
                       "Это многочастное сообщение в формате MIME.\r\n"
                       "\r\n",
                       sender, recipient, subject, boundary);
    
    offset += snprintf(mime_message + offset, total_size - offset,
                       "--%s\r\n"
                       "Content-Type: text/plain; charset=UTF-8\r\n"
                       "Content-Transfer-Encoding: 8bit\r\n"
                       "\r\n"
                       "%s\r\n"
                       "\r\n",
                       boundary, body);
    
    for (int i = 0; i < attachment_count; i++) {
        const char* filepath = attachments[i];
        const char* content_type = get_content_type(filepath);
        
        const char* filename = strrchr(filepath, '/');
        if (!filename) filename = filepath;
        else filename++;
        
        size_t encoded_length;
        char* encoded_content = encode_file_base64(filepath, &encoded_length);
        if (!encoded_content) {
            fprintf(stderr, "Предупреждение: Не удалось закодировать вложение %s\n", filepath);
            continue;
        }
        
        offset += snprintf(mime_message + offset, total_size - offset,
                           "--%s\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Transfer-Encoding: base64\r\n"
                           "Content-Disposition: attachment; filename=\"%s\"\r\n"
                           "\r\n",
                           boundary, content_type, filename);
        
        for (size_t j = 0; j < encoded_length; j += MAX_LINE_LENGTH) {
            size_t line_length = (encoded_length - j) > MAX_LINE_LENGTH ? MAX_LINE_LENGTH : (encoded_length - j);
            offset += snprintf(mime_message + offset, total_size - offset, "%.*s\r\n", 
                               (int)line_length, encoded_content + j);
        }
        
        offset += snprintf(mime_message + offset, total_size - offset, "\r\n");
        free(encoded_content);
    }
    
    offset += snprintf(mime_message + offset, total_size - offset, "--%s--\r\n.\r\n", boundary);
    
    return mime_message;
}