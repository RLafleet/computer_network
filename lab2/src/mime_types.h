#ifndef MIME_TYPES_H
#define MIME_TYPES_H

const char *get_mime_type(const char *filename);
int is_binary_file(const char *filename);

#endif // MIME_TYPES_H