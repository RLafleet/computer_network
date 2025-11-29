#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define stat _stat
#else
#include <unistd.h>
#endif

int main() {
    const char* filepath = "..\\www\\index.html";
    
    printf("Testing file access for: %s\n", filepath);
    
    // Convert to absolute path
    char abs_path[512];
#ifdef _WIN32
    if (_fullpath(abs_path, filepath, sizeof(abs_path)) == NULL) {
        printf("Failed to get absolute path\n");
        return 1;
    }
#else
    if (realpath(filepath, abs_path) == NULL) {
        printf("Failed to get absolute path\n");
        return 1;
    }
#endif
    
    printf("Absolute path: %s\n", abs_path);
    
    struct stat file_stat;
    if (stat(abs_path, &file_stat) == 0) {
        printf("File exists! Size: %ld bytes\n", file_stat.st_size);
    } else {
        printf("File does not exist or cannot be accessed\n");
        perror("stat");
    }
    
    return 0;
}