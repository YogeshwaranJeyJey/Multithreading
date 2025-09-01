#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static ssize_t write_all(int fd, const void *buffer, size_t count)
{
    const uint8_t *pointer = buffer;
    size_t left = count;

    while (left > 0)
    {
        ssize_t bytes_written = write(fd, pointer, left);

        if (bytes_written < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        left -= (size_t)bytes_written;
        pointer += bytes_written;
    }

    return (ssize_t)count;
}

int main()
{
    const char *srcFilePath = "records.bin";
    const char *destFilePath = "copy.bin";

    int fd_src = open(srcFilePath, O_RDONLY, 0644);

    if (fd_src < 0)
    {
        perror("open-src");
        return 1;
    }

    int fd_destination = open(destFilePath, O_CREAT | O_WRONLY | O_TRUNC, 0644);

    if (fd_destination < 0)
    {
        perror("open-dest");
        return 1;
    }

    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = read(fd_src, buffer, sizeof(buffer))) > 0)
    {
        if (write_all(fd_destination, buffer, bytesRead) != bytesRead)
        {
            perror("write");
            close(fd_src);
            close(fd_destination);
            return 1;
        }
        if (bytesRead < 0)
        {
            perror("read");
        }

        close(fd_src);
        close(fd_destination);

        printf("File copied from %s → %s\n", srcFilePath, destFilePath);
        return 0;
    }
}