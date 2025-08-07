#ifndef SLABALLOCATOR_H
#define SLABALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLAB_SIZE 4096
#define MAXIMUM_OBJECTS_PER_SLAB 64

typedef struct object_header
{
    struct object_header *next_free;
} object_header_t;

typedef struct slab
{
    void *memory;
    object_header_t *free_list;
    size_t object_size;
    size_t objects_per_slab;
    size_t free_objects;
    struct slab *next;
} slab_t;

typedef struct slab_cache
{
    slab_t *slabs;
    size_t object_size;
    char name[64];
} slab_cache_t;

#endif