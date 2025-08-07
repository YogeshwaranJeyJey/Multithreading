#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H

#include "main.h"

//Type/Struct Definition
typedef struct memory_pool {
    char* memory;   // Primary memory block
    size_t size;    // Size of the pool
    size_t used;    // Allocated storage (bytes)
    size_t block_size;   // Size of each allocation
} memory_pool_t;

memory_pool_t* pool_create(size_t total_size, size_t block_size);
void* pool_alloc(memory_pool_t *pool) ;
void pool_reset(memory_pool_t *pool);
void pool_destroy(memory_pool_t *pool);
void dbms_scenario();

#endif