#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct entry {
    char *key;
    void *value;
    struct entry *next;
} entry_t;

typedef struct bucket {
    pthread_mutex_t lock;
    entry_t *head;
} bucket_t;

typedef struct concurrentHashMap {
    size_t nbuckets;
    bucket_t *buckets;
    atomic_size_t count;
} concurrentHashMap_t;

static uint64_t hash_str(const char *s) {
    uint64_t hash = 5381;
    int c;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + (unsigned char)c;
    return hash;
}

concurrentHashMap_t *concurrentHashMap_create(size_t nbuckets) {
    if (nbuckets == 0) return NULL;
    concurrentHashMap_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->nbuckets = nbuckets;
    m->buckets = calloc(nbuckets, sizeof(bucket_t));
    if (!m->buckets) {
        free(m);
        return NULL;
    }
    for (size_t i = 0; i < nbuckets; ++i) {
        pthread_mutex_init(&m->buckets[i].lock, NULL);
        m->buckets[i].head = NULL;
    }
    atomic_init(&m->count, 0);
    return m;
}

void *concurrentHashMap_insert(concurrentHashMap_t *m, const char *key, void *value) {
    if (!m || !key) return NULL;
    uint64_t h = hash_str(key);
    size_t idx = (size_t)(h % m->nbuckets);
    bucket_t *b = &m->buckets[idx];

    pthread_mutex_lock(&b->lock);
    entry_t *e = b->head;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            void *old = e->value;
            e->value = value;
            pthread_mutex_unlock(&b->lock);
            return old;
        }
        e = e->next;
    }
    entry_t *ne = malloc(sizeof(*ne));
    if (!ne) {
        pthread_mutex_unlock(&b->lock);
        return NULL;
    }
    ne->key = strdup(key);
    ne->value = value;
    ne->next = b->head;
    b->head = ne;
    atomic_fetch_add(&m->count, 1);
    pthread_mutex_unlock(&b->lock);
    return NULL;
}

void *concurrentHashMap_get(concurrentHashMap_t *m, const char *key) {
    if (!m || !key) return NULL;
    uint64_t h = hash_str(key);
    size_t idx = (size_t)(h % m->nbuckets);
    bucket_t *b = &m->buckets[idx];

    pthread_mutex_lock(&b->lock);
    entry_t *e = b->head;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            void *val = e->value;
            pthread_mutex_unlock(&b->lock);
            return val;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&b->lock);
    return NULL;
}

void *concurrentHashMap_remove(concurrentHashMap_t *m, const char *key) {
    if (!m || !key) return NULL;
    uint64_t h = hash_str(key);
    size_t idx = (size_t)(h % m->nbuckets);
    bucket_t *b = &m->buckets[idx];

    pthread_mutex_lock(&b->lock);
    entry_t *prev = NULL, *e = b->head;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            if (prev) prev->next = e->next; else b->head = e->next;
            void *val = e->value;
            free(e->key);
            free(e);
            atomic_fetch_sub(&m->count, 1);
            pthread_mutex_unlock(&b->lock);
            return val;
        }
        prev = e;
        e = e->next;
    }
    pthread_mutex_unlock(&b->lock);
    return NULL;
}

void concurrentHashMap_destroy(concurrentHashMap_t *m, void (*free_value)(void *)) {
    if (!m) return;
    for (size_t i = 0; i < m->nbuckets; ++i) {
        bucket_t *b = &m->buckets[i];
        pthread_mutex_lock(&b->lock);
        entry_t *e = b->head;
        while (e) {
            entry_t *nx = e->next;
            if (free_value && e->value) free_value(e->value);
            free(e->key);
            free(e);
            e = nx;
        }
        b->head = NULL;
        pthread_mutex_unlock(&b->lock);
        pthread_mutex_destroy(&b->lock);
    }
    free(m->buckets);
    free(m);
}

typedef struct thread_arg {
    concurrentHashMap_t *map;
    int tid;
    int nkeys;
} thread_arg_t;

void *worker_insert(void *arg) {
    thread_arg_t *ta = arg;
    char keybuf[64];
    for (int i = 0; i < ta->nkeys; ++i) {
        snprintf(keybuf, sizeof(keybuf), "t%d-k%d", ta->tid, i);
        int *v = malloc(sizeof(int));
        *v = ta->tid * 100000 + i;
        void *old = concurrentHashMap_insert(ta->map, keybuf, v);
        if (old) {
            free(old);
        }
    }
    return NULL;
}

void *worker_lookup(void *arg) {
    thread_arg_t *ta = arg;
    char keybuf[64];
    int found = 0;
    for (int i = 0; i < ta->nkeys; ++i) {
        snprintf(keybuf, sizeof(keybuf), "t%d-k%d", ta->tid, i);
        void *val = concurrentHashMap_get(ta->map, keybuf);
        if (val) ++found;
    }
    printf("Lookup thread %d found %d/%d keys it searched (may be 0 if lookups ran before inserts)\n",
           ta->tid, found, ta->nkeys);
    return NULL;
}

int main(void) {
    const int NTHREADS = 8;
    const int KEYS_PER_THREAD = 1000;
    concurrentHashMap_t *m = concurrentHashMap_create(256);
    if (!m) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    pthread_t inserters[NTHREADS];
    pthread_t lookers[NTHREADS];
    thread_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; ++i) {
        args[i].map = m;
        args[i].tid = i;
        args[i].nkeys = KEYS_PER_THREAD;
        pthread_create(&inserters[i], NULL, worker_insert, &args[i]);
    }

    for (int i = 0; i < NTHREADS; ++i) {
        pthread_create(&lookers[i], NULL, worker_lookup, &args[i]);
    }

    for (int i = 0; i < NTHREADS; ++i) pthread_join(inserters[i], NULL);
    for (int i = 0; i < NTHREADS; ++i) pthread_join(lookers[i], NULL);

    size_t total = atomic_load(&m->count);
    printf("Total entries after inserts: %" PRIuPTR " (expected %d)\n", (uintptr_t)total, NTHREADS * KEYS_PER_THREAD);

    for (int t = 0; t < NTHREADS; ++t) {
        char key[64];
        snprintf(key, sizeof(key), "t%d-k%d", t, 42);
        int *pv = concurrentHashMap_remove(m, key);
        if (pv) {
            printf("Removed %s -> %d\n", key, *pv);
            free(pv);
        } else {
            printf("Key %s not found\n", key);
        }
    }
    concurrentHashMap_destroy(m, free);
    return 0;
}
