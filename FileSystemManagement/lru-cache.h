#ifndef LRU_CACHE
#define LRU_CACHE

#define CACHE_SIZE 5
#define KEY_SIZE 32
#define DATA_SIZE 256

#include "main.h"

typedef struct cache_node {
    char key[KEY_SIZE];
    char data[DATA_SIZE];
    struct cache_node *prev;
    struct cache_node *next;
}cache_node_t;

typedef struct lru_cache {
    cache_node_t *head; //MRU
    cache_node_t *tail; //LRU
    cache_node_t *nodes[CACHE_SIZE];
    int count;
    int capacity;
}lru_cache_t;

unsigned int hash(const char *key);
lru_cache_t* create_lru_cache();
void move_to_front(lru_cache_t *cache, cache_node_t *node);
void add_to_front(lru_cache_t *cache, cache_node_t *node);
cache_node_t* remove_lru(lru_cache_t *cache);
char* cache_get(lru_cache_t *cache, const char *key);
void cache_put(lru_cache_t *cache, const char *key, const char *data);

#endif