#ifndef MIME_UTILS_H
#define MIME_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 76
#define MAX_BOUNDARY_LENGTH 70

typedef struct {
    char* filename;
    char* content_type;
    char* transfer_encoding;
} attachment_info_t;

char* generate_boundary(void);
char* get_content_type(const char* filename);
char* base64_encode(const unsigned char* data, size_t input_length, size_t* output_length);
char* encode_file_base64(const char* filepath, size_t* encoded_length);
char* create_mime_message(const char* sender, const char* recipient,
                         const char* subject, const char* body,
                         char** attachments, int attachment_count,
                         const char* boundary);
void print_progress(size_t current, size_t total);

#endif