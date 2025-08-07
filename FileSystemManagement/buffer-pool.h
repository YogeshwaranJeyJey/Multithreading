#ifndef BUFFERPOOL.H
#define BUFFERPOOL_H

#include "main.h"

#define BUFFER_SIZE 4096
#define POOL_SIZE 5

// Buffer
typedef struct buffer {
    char data[BUFFER_SIZE];
    bool is_free;
    int buffer_id;
    struct buffer *next; 
}buffer_t;

// Buffer Pool manager
typedef struct buffer_pool {
    buffer_t *buffers;
    buffer_t *free_list;
    int total_buffers;
    int free_count;
}buffer_pool_t;

buffer_pool_t* create_buffer_pool(int pool_size);
buffer_t* acquire_buffer(buffer_pool_t *pool);
void release_buffer(buffer_pool_t *pool, buffer_t *buffer);
void destroy_buffer_pool(buffer_pool_t *pool);

#endif
