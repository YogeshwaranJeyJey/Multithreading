#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>

int main(){
    int fd = open("example.txt", O_RDWR, 0644);
    if(fd == -1){
        perror("open");
        return 1;
    }
    printf("File opened with file descriptor : %d\n", fd);
    char *msg = "Inserted msg!";
    if(lseek(fd, 5, SEEK_SET) == -1){
        perror("Lseek");
        close(fd);
        return 1;
    }
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