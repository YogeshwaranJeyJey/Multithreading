#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>

int main(){
    int fd = open("example.txt", O_CREAT| O_WRONLY| O_TRUNC, 0644);
    if(fd == -1){
        perror("open");
        return 1;
    }
    printf("File opened with file descriptor : %d\n", fd);
    char *msg = "Hello world (added by write())";
    ssize_t bytesWritten = write(fd, msg, strlen(msg));
    if(bytesWritten == -1){
        perror("Write");
        close(fd);
        return 1;
    }
    printf("Wrote %zd bytes successfully!\n", bytesWritten);
    close(fd);
    return 0;
}