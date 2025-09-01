#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>

int main(){
    int fd = open("example.txt", O_RDONLY, 0644);
    if(fd == -1){
        perror("open");
        return 1;
    }
    printf("File opened with file descriptor : %d\n", fd);
    char buffer[128];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if(bytesRead == -1){
        perror("Read");
        close(fd);
        return 1;
    }
    buffer[bytesRead] = '\0';
    printf("Read %zd bytes : %s\n", bytesRead, buffer);
    close(fd);
    return 0;
}