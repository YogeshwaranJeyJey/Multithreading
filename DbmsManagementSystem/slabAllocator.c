#include "slabAllocator.h"

// Creating a new slab
slab_t *slab_create(size_t object_size)
{
    slab_t *slab = malloc(sizeof(slab_t));

    if (!slab)
        return NULL;

    slab->memory = aligned_alloc(4096, SLAB_SIZE); // Align the objects in a page

    slab->object_size = object_size;
    slab->objects_per_slab = SLAB_SIZE / object_size;
    slab->free_objects = slab->objects_per_slab;
    slab->next = NULL;

    slab->free_list = NULL;
    char *obj_ptr = (char *)slab->memory;

    for (size_t iterator = 0; iterator < slab->objects_per_slab; iterator++)
    {
        object_header_t *header = (object_header_t *)obj_ptr;
        header->next_free = slab->free_list;
        slab->free_list = header;
        obj_ptr += object_size;
    }

    printf("Created a slab with %zu objects of size %zu \n", slab->objects_per_slab, object_size);
    return slab;
}

// Creating a slab cache
slab_cache_t *slab_cache_create(const char *name, size_t object_size)
{
    slab_cache_t *cache = malloc(sizeof(slab_cache_t));
    if (!cache)
        return NULL;

    cache->slabs = NULL;
    cache->object_size = object_size;
    strncpy(cache->name, name, sizeof(cache->name) - 1);

    printf("Created slab cache '%s' for the %zu bytes of objects \n", name, object_size);
    return cache;
}

// Allocating object
void *slab_alloc(slab_cache_t *cache)
{
    slab_t *slab = cache->slabs;

    while (slab && slab->free_objects == 0)
    {
        slab = slab->next;
    }

    if (!slab)
    {
        slab = slab_create(cache->object_size);
        if (!slab)
            return NULL;

        slab->next = cache->slabs;
        cache->slabs = slab;
    }

    object_header_t *obj = slab->free_list;
    slab->free_list = obj->next_free;
    slab->free_objects--;

    return obj;
}

// Unallocating object (free)
void slab_free(slab_cache_t *cache, void *obj)
{
    slab_t *slab = cache->slabs;
    while (slab)
    {
        char *slab_start = (char *)slab->memory;
        char *slab_end = slab_start + SLAB_SIZE;

        if ((char *)obj >= slab_start && (char *)obj < slab_end)
        {
            object_header_t *header = (object_header_t *)obj;
            header->next_free = slab->free_list;
            slab->free_list = header;
            slab->free_objects++;
            return;
        }

        slab = slab->next;
    }
}

// DB Tuple Usecase scenario
typedef struct db_tuple
{
    int id;
    char data[128]; // 128 bytes
} db_tuple_t;

void dbms_scenario()
{
    slab_cache_t *tuple_cache = slab_cache_create("db_tuples", sizeof(db_tuple_t));

    db_tuple_t *tuples[1000];

    for (int iterator = 0; iterator < 1000; iterator++)
    {
        tuples[iterator] = (db_tuple_t *)slab_alloc(tuple_cache);

        if (tuples[iterator])
        {
            tuples[iterator]->id = iterator;
            snprintf(tuples[iterator]->data, sizeof(tuples[iterator]->data), "Tuple %d", iterator);
        }
    }

    for (int iterator = 0; iterator < 500; iterator++)
    {
        slab_free(tuple_cache, tuples[iterator]);
    }

    printf("500 Tuple objects' memory are unallocated \n");
}