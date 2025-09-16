#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define QUEUE_CAPACITY 5
#define NUM_PRODUCERS  2
#define NUM_CONSUMERS  2
#define ITEMS_PER_PRODUCER 10

typedef struct {
    int *buffer;
    int capacity;
    int count;
    int in, out;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} BoundedQueue;

void queue_init(BoundedQueue *q, int capacity) {
    q->buffer = malloc(sizeof(int) * capacity);
    q->capacity = capacity;
    q->count = 0;
    q->in = 0;
    q->out = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void queue_destroy(BoundedQueue *q) {
    free(q->buffer);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

void queue_enqueue(BoundedQueue *q, int item) {
    pthread_mutex_lock(&q->mutex);

    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->buffer[q->in] = item;
    q->in = (q->in + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int queue_dequeue(BoundedQueue *q) {
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    int item = q->buffer[q->out];
    q->out = (q->out + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return item;
}

BoundedQueue queue;
int total_items = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
int consumed_count = 0;

void *producer(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int item = id * 1000 + i;
        usleep((rand() % 200) * 1000);
        queue_enqueue(&queue, item);
        printf("Producer %d produced %d\n", id, item);
    }
    return NULL;
}

void *consumer(void *arg) {
    int id = *(int *)arg;
    while (1) {
        pthread_mutex_lock(&queue.mutex);
        if (consumed_count >= total_items) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }
        pthread_mutex_unlock(&queue.mutex);

        int item = queue_dequeue(&queue);
        pthread_mutex_lock(&queue.mutex);
        consumed_count++;
        pthread_mutex_unlock(&queue.mutex);

        printf("Consumer %d consumed %d (total consumed = %d)\n",
               id, item, consumed_count);

        usleep((rand() % 300) * 1000);
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    queue_init(&queue, QUEUE_CAPACITY);

    pthread_t producers[NUM_PRODUCERS], consumers[NUM_CONSUMERS];
    int prod_ids[NUM_PRODUCERS], cons_ids[NUM_CONSUMERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i + 1;
        pthread_create(&producers[i], NULL, producer, &prod_ids[i]);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cons_ids[i] = i + 1;
        pthread_create(&consumers[i], NULL, consumer, &cons_ids[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    printf("All producers and consumers finished. Total consumed = %d\n", consumed_count);

    queue_destroy(&queue);
    return 0;
}