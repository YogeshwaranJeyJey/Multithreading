#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<time.h>

#define BUFFER_SIZE 5
#define PRODUCE_PER_PRODUCER 10
#define NUMBER_OF_PRODUCERS 2
#define NUMBER_OF_CONSUMERS 2

//shared buffer
int buffer[BUFFER_SIZE];
int in = 0;         // insert index
int out = 0;        // remove index
int count = 0;      // number of items in the buffer

//helpers for termination
int total_to_produce = PRODUCE_PER_PRODUCER * NUMBER_OF_PRODUCERS;
int consumed_total = 0;

//sync primitives
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

//producer
void *producer(void *arg)
{
    int id = *(int *)arg;

    for (int iterator = 0; iterator < PRODUCE_PER_PRODUCER; ++iterator)
    {
        usleep((rand() % 200) * 1000 ); //0-200ms

        pthread_mutex_lock(&mutex);

        while(count == BUFFER_SIZE)
        {
            printf("Producer %d : buffer full, waiting.. \n", id);
            pthread_cond_wait(&not_full, &mutex);
        }

        int item = id * 1000 + iterator;
        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;
        ++count;

        printf("Producer %d : produced %d (count = %d) \n", id, item, count);

        pthread_cond_signal(&not_empty);    //wake up one consumer

        pthread_mutex_unlock(&mutex);
    }

    printf("Producer %d : done producing..\n", id);
    return NULL;
}

//consumer
void *consumer(void *arg)
{
    int id = *(int *)arg;

    while(1)
    {
        pthread_mutex_lock(&mutex);

        while(count == 0)
        {
            if(consumed_total >= total_to_produce)
            {
                pthread_mutex_unlock(&mutex);
                printf("Consumer %d : no more items, exiting...\n", id);
                return NULL;
            }

            printf("Consumer %d : buffer is empty, waiting...\n", id);
            pthread_cond_wait(&not_empty, &mutex);
        }

        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        --count;
        ++consumed_total;

        printf("Consumer %d : consumed %d items (consumed_total = %d, count = %d)\n", id, item, consumed_total, count);

        pthread_cond_signal(&not_full);     //wakes up one producer

        pthread_mutex_unlock(&mutex);

        usleep((rand() % 300) * 1000 );     //0-300ms

        if(consumed_total >= total_to_produce)
        {
            printf("Consumer %d : consumed final item, exiting..\n", id);
            return NULL;
        }
    }
}

//main
int main(void)
{
    srand((unsigned)time (NULL));
    pthread_t producers[NUMBER_OF_PRODUCERS], consumers[NUMBER_OF_CONSUMERS];
    int ids_producers[NUMBER_OF_PRODUCERS], ids_consumers[NUMBER_OF_CONSUMERS];

    for(int iterator = 0; iterator < NUMBER_OF_PRODUCERS; ++iterator)
    {
        ids_producers[iterator] = iterator + 1;
        if(pthread_create(&producers[iterator], NULL, producer, &ids_producers[iterator]) != 0)
        {
            perror("pthread_create producer");
            exit(EXIT_FAILURE);
        }
    }

    for(int iterator = 0; iterator < NUMBER_OF_CONSUMERS; ++iterator)
    {
        ids_consumers[iterator] = iterator + 1;
        if(pthread_create(&consumers[iterator], NULL, consumer, &ids_consumers[iterator]) != 0)
        {
            perror("pthread_create consumer");
            exit(EXIT_FAILURE);
        }
    }

    for(int iterator = 0; iterator < NUMBER_OF_PRODUCERS; ++iterator)
    {
        pthread_join(producers[iterator], NULL);
    }

    for(int iterator = 0; iterator < NUMBER_OF_CONSUMERS; ++iterator)
    {
        pthread_join(consumers[iterator], NULL);
    }

    printf("All producers and consumers are finished executing \n Consumed total = %d (Expected = %d)\n", consumed_total, total_to_produce);

    return 0;
}