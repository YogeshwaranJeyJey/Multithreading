#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_THREADS 4

typedef struct {
    char *data;
    size_t start;
    size_t end;
    long count;
} ThreadArg;

int is_word_char(char c) {
    return isalnum((unsigned char)c);
}

void* count_words(void *arg) {
    ThreadArg *targ = (ThreadArg*) arg;
    char *data = targ->data;

    long count = 0;
    int in_word = 0;

    for (size_t i = targ->start; i < targ->end; i++) {
        if (is_word_char(data[i])) {
            if (!in_word) {
                count++;
                in_word = 1;
            }
        } else {
            in_word = 0;
        }
    }

    targ->count = count;
    return NULL;
}

long parallel_word_count(const char *filename, int num_threads) {
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
    size_t filesize = sb.st_size;

    char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    pthread_t threads[num_threads];
    ThreadArg args[num_threads];

    size_t chunk = filesize / num_threads;

    for (int i = 0; i < num_threads; i++) {
        args[i].data = data;
        args[i].start = i * chunk;
        args[i].end = (i == num_threads - 1) ? filesize : (i + 1) * chunk;
        args[i].count = 0;

        if (i > 0) {
            while (args[i].start < filesize && is_word_char(data[args[i].start])) {
                args[i].start++;
            }
        }

        pthread_create(&threads[i], NULL, count_words, &args[i]);
    }
    long total = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total += args[i].count;
    }

    munmap(data, filesize);
    close(fd);

    return total;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    long count = parallel_word_count(argv[1], MAX_THREADS);
    printf("Total words: %ld\n", count);

    return 0;
}
