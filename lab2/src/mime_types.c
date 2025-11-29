#include "mime_types.h"
#include <string.h>
#include <ctype.h>

const char *get_mime_type(const char *filename)
{
    if (!filename)
        return "application/octet-stream";

    const char *dot = strrchr(filename, '.');
    if (!dot)
        return "application/octet-stream";

    dot++;

    if (strcasecmp(dot, "html") == 0 || strcasecmp(dot, "htm") == 0)
        return "text/html";
    else if (strcasecmp(dot, "css") == 0)
        return "text/css";
    else if (strcasecmp(dot, "js") == 0)
        return "application/javascript";
    else if (strcasecmp(dot, "json") == 0)
        return "application/json";
    else if (strcasecmp(dot, "xml") == 0)
        return "application/xml";
    else if (strcasecmp(dot, "txt") == 0)
        return "text/plain";
    else if (strcasecmp(dot, "jpg") == 0 || strcasecmp(dot, "jpeg") == 0)
        return "image/jpeg";
    else if (strcasecmp(dot, "png") == 0)
        return "image/png";
    else if (strcasecmp(dot, "gif") == 0)
        return "image/gif";
    else if (strcasecmp(dot, "bmp") == 0)
        return "image/bmp";
    else if (strcasecmp(dot, "ico") == 0)
        return "image/x-icon";
    else if (strcasecmp(dot, "svg") == 0)
        return "image/svg+xml";
    else if (strcasecmp(dot, "pdf") == 0)
        return "application/pdf";
    else if (strcasecmp(dot, "zip") == 0)
        return "application/zip";
    else if (strcasecmp(dot, "mp3") == 0)
        return "audio/mpeg";
    else if (strcasecmp(dot, "mp4") == 0)
        return "video/mp4";
    else if (strcasecmp(dot, "avi") == 0)
        return "video/x-msvideo";
    else if (strcasecmp(dot, "mov") == 0)
        return "video/quicktime";

    return "application/octet-stream";
}

int is_binary_file(const char *filename)
{
    if (!filename)
        return 1;

    const char *mime_type = get_mime_type(filename);

    if (strncmp(mime_type, "text/", 5) == 0)
        return 0;

    if (strcmp(mime_type, "application/javascript") == 0 ||
        strcmp(mime_type, "application/json") == 0 ||
        strcmp(mime_type, "application/xml") == 0 ||
        strcmp(mime_type, "image/svg+xml") == 0)
        return 0;

    return 1;
}

static int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}