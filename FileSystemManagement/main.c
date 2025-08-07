#include "buffer-pool.h"

int main(){
    FILE *fp = fopen("file1.txt", "r");
    buffer_pool_t* pool = create_buffer_pool(POOL_SIZE);
}