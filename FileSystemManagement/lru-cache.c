#include "lru-cache.h"

//hashing
unsigned int hash(const char *key)
{
    unsigned int hash_value = 0;
    while (*key)
    {
        hash_value = (hash_value * 31) + *key++ ;
    }

    return hash_value % CACHE_SIZE;
}

//Initialize LRU cache
lru_cache_t* create_lru_cache(){
    lru_cache_t *cache = malloc(sizeof(lru_cache_t));
    if(!cache) return NULL;

    cache->head = NULL;
    cache->tail = NULL;
    cache->count = 0;
    cache->capacity = CACHE_SIZE;

    for (int iterator = 0; iterator < CACHE_SIZE; iterator++)
    {
        cache->nodes[iterator] = NULL;
    }

    return cache;
}

//Push the Node to front (to mark it as MRU)
void move_to_front(lru_cache_t *cache, cache_node_t *node)
{
    if(cache->head == node) return;

    if(node->prev) node->prev->next = node->next;
    if(node->next) node->next->prev = node->prev;
    if(cache->tail == node) cache->tail = node->prev;

    node->prev = NULL;
    node->next = cache->head;
    if(cache->head) cache->head->prev = node;
    cache->head = node;

    if(!cache->tail) cache->tail = node;

}

//Add Node at the front
void add_to_front(lru_cache_t *cache, cache_node_t *node){
    node->prev = NULL;
    node->next = cache->head;

    if(cache->head)
    {
        cache->head->prev = node;
    }
    cache->head = node;

    if(!cache->tail)
    {
        cache->tail = node;
    }
}

//Remove LRU Node
cache_node_t* remove_lru(lru_cache_t *cache)
{
    if(!cache->tail) return NULL;

    cache_node_t *lru = cache->tail;

    if(cache->tail->prev)
    {
        cache->tail->prev->next = NULL;
    }else
    {
        cache->head = NULL;
    }

    cache->tail = cache->tail->prev;

    return lru;
}

// Getter - to get data from the cache
char* cache_get(lru_cache_t *cache, const char *key) {
    unsigned int index = hash(key);
    cache_node_t *current = cache->nodes[index];

    while (current)
    {
        if(strcmp(current->key, key) == 0)
        {
            move_to_front(cache, current);
            printf("Cache successfully HIT for the key %s \n", key);
            return current->data;
        }
        current = current->next;
    }
    printf("Cache MISS for the key %s \n", key);
    return NULL;
}

// Put - to put the data to the cache
void cache_put(lru_cache_t *cache, const char *key, const char *data)
{
    unsigned int index = hash(key);

    char *existing = cache_get(cache, key);
    if(existing)
    {
        cache_node_t *current = cache->nodes[index];
        while (current && strcmp(current->key, key) != 0) {
            current = current->next;
        }
        if(current)
        {
            strncpy(current->data, data, DATA_SIZE - 1);
            current->data[DATA_SIZE - 1] = '\0';
        }
        return;
    }

    cache_node_t *new_node = malloc(sizeof(cache_node_t));
    if(!new_node) return;

    strncpy(new_node->key, key, KEY_SIZE - 1);
    new_node->key[KEY_SIZE - 1] = '\0';

    strncpy(new_node->data, data, DATA_SIZE - 1);
    new_node->data[DATA_SIZE - 1] = '\0';

    new_node->prev = new_node->next = NULL;

    if(cache->count >= cache->capacity){
        cache_node_t *lru = remove_lru(cache);

        if(lru)
        {
            unsigned int lru_index = hash(lru->key);
            if(cache->nodes[lru_index] == lru) {
                cache->nodes[lru_index] = lru->next;
            }
            free(lru);
            cache->count--;
        }
    }

    add_to_front(cache, new_node);
    new_node->next = cache->nodes[index];
    cache->nodes[index] = new_node;
    cache->count++;

    printf("Cached key is %s with the data %s \n", key, data);

}
