#include "memoryPool.h"

//Memory Pool Creation
memory_pool_t* pool_create(size_t total_size, size_t block_size){

    memory_pool_t *pool = malloc(sizeof(memory_pool_t));
    if (!pool) return NULL;

    pool->memory = malloc(total_size);
    if(!pool->memory)
    {
       free(pool);
       return NULL; 
    }

    pool->size = total_size;
    pool->used = 0;
    pool->block_size = block_size;

    printf("Created memory pool : %zu bytes, block size : %zu \n", total_size, block_size);
    return pool;
}

//Memory Pool Allocation
void* pool_alloc(memory_pool_t *pool) {
    if (pool->used + pool->block_size > pool->size)
    {
        printf("Memory pool is exhausted! \n");
        return NULL;
    }

    void *ptr = pool->memory + pool->used;
    pool->used += pool->block_size;
    return ptr;
}

//Resetting the memory pool
void pool_reset(memory_pool_t *pool)
{
    pool->used = 0;
    printf("Pool reset - All memory are free now");
}

//Destroying the memory pool
void pool_destroy(memory_pool_t *pool)
{
    if (pool)
    {
        free(pool->memory);
        free(pool);
    }
}

// A DBMS usecase scenario
void dbms_scenario()
{
    memory_pool_t *record_pool = pool_create(1024 * 1024, 256);  //1MB memory pool (4096 records)

    for (int iterator = 0; iterator < 100; iterator++)
    {
        void *record = pool_alloc(record_pool);

        if (record)
        {
            snprintf((char*)record, 256, "Record %d data", iterator);
        }
    }

    pool_reset(record_pool);

    pool_destroy(record_pool);
}



// It pre-allocates a chunk of memory and manages smaller allocations within them
// Predictive (performance) behaviour - no overkill like malloc
// Reduced possibilites of Fragmentation
// Bulk operations - reset the entire memory chunk
// Sequential allocations - Caching
