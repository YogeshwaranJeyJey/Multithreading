#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void search_text(const char *filename, const char *pattern) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (sb.st_size == 0) {
        printf("Empty file.\n");
        close(fd);
        return;
    }

    char *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    char *match = data;
    size_t pat_len = strlen(pattern);
    int found = 0;

    while ((match = memmem(match, (data + sb.st_size) - match, pattern, pat_len))) {
        size_t offset = match - data;
        printf("Found at offset %zu\n", offset);
        match += pat_len;
        found = 1;
    }

    if (!found) {
        printf("Pattern not found.\n");
    }

    munmap(data, sb.st_size);
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <pattern>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    search_text(argv[1], argv[2]);

    return 0;
}
