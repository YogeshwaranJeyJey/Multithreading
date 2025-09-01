#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>

int main(){
    int fd = open("example.txt", O_CREAT| O_WRONLY| O_TRUNC, 0644 );
    if(fd == -1){
        perror("open");
        return 1;
    }
    printf("File opened with file descriptor : %d\n", fd);
    close(fd);
    return 0;
}