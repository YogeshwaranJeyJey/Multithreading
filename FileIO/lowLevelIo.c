#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<stddef.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/stat.h>
#include<errno.h>

#pragma pack(push,1)
typedef struct {
    uint32_t id;
    double value;
    char name[32];
}Record;
#pragma pack(pop)

static ssize_t read_all(int fd, void *buffer, size_t count)
{
    uint8_t *pointer = buffer;
    size_t left = count;

    while (left > 0)
    {
        ssize_t bytes_read = read(fd, pointer, left);

        if(bytes_read < 0)
        {
            if(errno == EINTR) continue;
            return -1;
        }

        if(bytes_read == 0)
        {
            break; //EndOfFile
        }

        left -= (size_t)bytes_read;
        pointer += bytes_read;
    }

    return (ssize_t)(count - left);
}

static ssize_t write_all(int fd, const void *buffer, size_t count)
{
    const uint8_t *pointer = buffer;
    size_t left = count;

    while (left > 0)
    {
        ssize_t bytes_written = write(fd, pointer, left);

        if(bytes_written < 0)
        {
            if(errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)bytes_written;
        pointer += bytes_written;
    }

    return (ssize_t)count;
}

int main()
{
    const char *fpath = "records.bin";

    Record records[3] = {
        {1, 3.14123, "Aaa"},
        {2, 2.14123, "Bbb"},
        {3, 1.14123, "Ccc"}
    };

    uint32_t n = 3; 

    //create/open/truncate the file
    int fd = open(fpath, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    //write the records data to the file
    //Writing the header first, n = 3 (4 bytes)
    if(write_all(fd, &n, sizeof(n)) != ((ssize_t)sizeof(n)))
    {
        perror("write header");
        close(fd);
        return 1;
    }
    //Writing the records
    for (size_t iterator = 0; iterator < n; ++iterator)
    {
        if(write_all(fd, &records[iterator], sizeof(Record)) != (ssize_t)sizeof(Record))
        {
            perror("write record");
            close(fd);
            return 1;
        }
    }

    //WAL like durable updation
    if (_commit(fd) != 0)
    {
        perror("fsync");
        close(fd);
        return 1;
    }

    close(fd);

    //read the records from the file
    int fd_read = open(fpath, O_RDONLY);

    if(fd_read < 0)
    {
        perror("open");
        return 1;
    }

    uint32_t n_read;

    if(read_all(fd_read, &n_read, sizeof(n_read)) != (ssize_t)sizeof(n_read))
    {
        perror("read headers");
        close(fd_read);
        return 1;
    }

    printf("Read %u records :  \n", n_read);

    Record record;

    for(uint32_t iterator = 0; iterator < n_read; ++iterator)
    {
        if(read_all(fd_read, &record, sizeof(record)) != (ssize_t)sizeof(record))
        {
            perror("read records");
            close(fd_read);
            return 1;
        }

        printf(" records[%u] id = %u value = %f name = \"%s\" \n", iterator, record.id, record.value, record.name);
    }

    close(fd_read);

    //lseek to change a single byte inside the record
    int fd_rw = open(fpath, O_RDWR);

    if(fd_rw < 0)
    {
        perror("open rw");
        return 1;
    }

    off_t byte_offset = (off_t)sizeof(n) + (off_t)1 * (off_t)sizeof(Record) + (off_t)offsetof(Record, name) + 1;
    
    if(lseek(fd_rw, byte_offset, SEEK_SET) == (off_t)-1)
    {
        perror("lseek");
        close(fd);
        return 1;
    }

    char new_character = 'X';

    if(write_all(fd_rw, &new_character, 1) != 1)
    {
        perror("writing a single byte");
        close(fd);
        return 1;
    }

    if(_commit(fd_rw) != 0)
    {
        perror("fsync after modify");
        close(fd_rw);
        return 1;
    }

    close(fd_rw);

    //read records from file after modification
    int fd_read_again = open(fpath, O_RDONLY);

    if (fd_read_again < 0)
    {
        perror("open read again");
        return 1; 
    }

    if (read_all(fd_read_again, &n_read, sizeof(n_read)) != (ssize_t)sizeof(n_read)) 
    {
        perror("read header again");
        close(fd_read_again);
        return 1; 
    }

    printf("After modification:\n");

    for (uint32_t iterator = 0; iterator < n_read; ++iterator) {
        if (read_all(fd_read_again, &record, sizeof(record)) != (ssize_t)sizeof(record))
        {
            perror("read record2"); 
            close(fd_read_again); 
            return 1; 
        }

        printf("  rec[%u] id=%u value=%f name=\"%s\"\n", iterator, record.id, record.value, record.name);
    }

    close(fd_read_again);

    return 0;
}

//Use EINTR handling