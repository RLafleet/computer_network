#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stddef.h>

#define MAX_PATH_LEN 512

typedef struct FileInfo
{
    char filepath[MAX_PATH_LEN];
    size_t size;
    int exists;
} FileInfo;

int resolve_filepath(const char *root_dir, const char *filename, char *resolved_path, size_t path_size);
int get_file_info(const char *filepath, FileInfo *info);
int read_file_content(const char *filepath, char **content, size_t *content_size);
int is_safe_path(const char *root_dir, const char *filepath);

#endif // FILE_HANDLER_H