#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>
#define stat _stat

int resolve_filepath(const char *root_dir, const char *filename, char *resolved_path, size_t path_size)
{
    if (!root_dir || !filename || !resolved_path || path_size == 0)
        return -1;

    int len = snprintf(resolved_path, path_size, "%s\\%s", root_dir, filename);
    if (len < 0 || (size_t)len >= path_size)
        return -1;

    return 0;
}

int get_file_info(const char *filepath, FileInfo *info)
{
    if (!filepath || !info)
        return -1;

    memset(info, 0, sizeof(FileInfo));
    strncpy(info->filepath, filepath, sizeof(info->filepath) - 1);
    info->filepath[sizeof(info->filepath) - 1] = '\0';

    char abs_path[MAX_PATH_LEN];
    if (_fullpath(abs_path, filepath, sizeof(abs_path)) == NULL)
    {
        info->size = 0;
        info->exists = 0;
        return 0;
    }

    struct stat file_stat;
    if (stat(abs_path, &file_stat) == 0)
    {
        info->size = file_stat.st_size;
        info->exists = 1;
    }
    else
    {
        info->size = 0;
        info->exists = 0;
    }

    return 0;
}

int read_file_content(const char *filepath, char **content, size_t *content_size)
{
    if (!filepath || !content || !content_size)
        return -1;

    *content = NULL;
    *content_size = 0;

    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return -1;
    }

    long file_size = ftell(file);
    if (file_size < 0)
    {
        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return -1;
    }

    *content = (char *)malloc(file_size + 1);
    if (!*content)
    {
        fclose(file);
        return -1;
    }

    size_t bytes_read = fread(*content, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size)
    {
        free(*content);
        *content = NULL;
        return -1;
    }

    (*content)[file_size] = '\0';
    *content_size = (size_t)file_size;

    return 0;
}

int is_safe_path(const char *root_dir, const char *filepath)
{
    if (!root_dir || !filepath)
        return 0;

    char abs_root[MAX_PATH_LEN];
    char abs_file[MAX_PATH_LEN];

    if (_fullpath(abs_root, root_dir, sizeof(abs_root)) == NULL)
        return 0;

    if (_fullpath(abs_file, filepath, sizeof(abs_file)) == NULL)
        return 0;

    size_t root_len = strlen(abs_root);
    if (strncmp(abs_file, abs_root, root_len) != 0)
        return 0;

    const char *ptr = filepath;
    while ((ptr = strstr(ptr, "..")) != NULL)
    {
        if ((ptr == filepath || *(ptr - 1) == '/' || *(ptr - 1) == '\\') &&
            (*(ptr + 2) == '/' || *(ptr + 2) == '\\' || *(ptr + 2) == '\0'))
        {
            return 0;
        }
        ptr += 2;
    }

    return 1;
}