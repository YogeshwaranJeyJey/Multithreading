#include "buffer-pool.h"

// Initialize the buffer pool
buffer_pool_t* create_buffer_pool(int pool_size)
{
    buffer_pool_t *pool = malloc(sizeof(buffer_pool_t));
    if (!pool) return NULL;

    pool->buffers = malloc(sizeof(buffer_t) * pool_size);
    if (!pool->buffers)
    {
        free(pool);
        return NULL;
    }

    pool->total_buffers = pool_size;
    pool->free_count = pool_size;
    pool->free_list = &pool->buffers[0];


    for (int iterator = 0; iterator < pool_size; iterator++)
    {
        pool->buffers[iterator].is_free = true;
        pool->buffers[iterator].buffer_id = iterator;

        if(iterator == pool_size - 1)
        {
            pool->buffers[iterator].next = NULL;
        }
        else
        {
            pool->buffers[iterator].next = &pool->buffers[iterator + 1];
        }
    }

    return pool;
}

//Acquiring the buffer
buffer_t* acquire_buffer(buffer_pool_t *pool) {
    if(pool->free_count == 0)
    {
        printf("Buffer pool is already exhausted \n");
        return NULL;
    }

    buffer_t *buffer = pool->free_list;
    pool->free_list = buffer->next;
    buffer->is_free = false;
    buffer->next = NULL;
    pool->free_count--;

    printf("The acquired buffer is %d\n", buffer->buffer_id);
    return buffer;
}

//Release the buffer
void release_buffer(buffer_pool_t *pool, buffer_t *buffer)
{
    if(buffer->is_free || !buffer) return;

    memset(buffer->data, 0, BUFFER_SIZE);

    buffer->is_free = true;
    buffer->next = pool->free_list;
    pool->free_list = buffer;
    pool->free_count++;

    prinf("Released buffer is %d \n", buffer->buffer_id);
}

//Destroy the buffer pool
void destroy_buffer_pool(buffer_pool_t *pool)
{
    if(pool)
    {
        free(pool->buffers);
        free(pool);
    }
}