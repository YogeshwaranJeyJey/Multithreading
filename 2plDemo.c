#define _GNU_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>

#define NUMBER_OF_ACCOUNTS 8
#define INITIAL_BALANCE 1000
#define NUMBER_OF_THREADS 4
#define TRANSFERS_PER_THREAD 2000

typedef struct {
    int locked_ids[16];
    int locked_count;
}Tx;

static int accounts[NUMBER_OF_ACCOUNTS];
static pthread_mutex_t record_lock[NUMBER_OF_ACCOUNTS];
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

static int rnd(int a, int b)
{
    return a + rand() % (b - a + 1);
}

static int total_balance(void)
{
    int sum = 0;
    for (int iterator = 0; iterator < NUMBER_OF_ACCOUNTS; iterator++)
    {
        sum += accounts[iterator];
    }

    return sum;
}

//tx begin
static void tx_begin(Tx *transaction)
{
    transaction->locked_count = 0;
}

//tx lock_record
static void tx_lock_record(Tx *transaction, int id)
{
    for (int iterator = 0; iterator < transaction->locked_count; ++iterator)
    {
        if(transaction->locked_ids[iterator] == id)
        {
            return;
        }
    }

    pthread_mutex_lock(&record_lock[id]);
    transaction->locked_ids[transaction->locked_count++] = id;

}

//tx commit
static void tx_commit(Tx *transaction)
{
    for(int iterator = transaction->locked_count - 1; iterator >= 0; --iterator)
    {
        pthread_mutex_unlock(&record_lock[transaction->locked_ids[iterator]]);
    }
    transaction->locked_count = 0;
}

//tx transfer
static void tx_transfer(Tx *transaction, int from, int to, int amount, int t_id)
{
    int a = from, b = to;

    if (a == b)
    {
        return;
    }

    if (a > b)
    {
        int temp = a;
        a = b;
        b = temp;
    }

    //Growing
    tx_lock_record(transaction, a);

    if(rnd(0,9) == 0)
    {
        usleep(100);
    }

    tx_lock_record(transaction, b);

    if(accounts[from] >= amount)
    {
        accounts[from] -= amount;
        accounts[to] += amount;

        if(rnd(0,29) == 0)
        {
            usleep(100);
        }
    }

    //Shrinking
    tx_commit(transaction);

    if(rnd(0, 999) == 0)
    {
        pthread_mutex_lock(&print_lock);
        printf("[Transaction %d] : transfer %d -> %d done with amount %d \n", t_id, from, to, amount);
        pthread_mutex_unlock(&print_lock);
    }
}

//worker
static void *worker(void *arg)
{
    int t_id = (int)(intptr_t)arg;
    Tx transaction;

    for (int iterator = 0; iterator < TRANSFERS_PER_THREAD; ++iterator)
    {
        int from = rnd(0, NUMBER_OF_ACCOUNTS - 1);
        int to = rnd(0, NUMBER_OF_ACCOUNTS - 1);
        int amount = rnd(1,25);

        tx_begin(&transaction);
        tx_transfer(&transaction, from, to, amount, t_id);
    }

    return NULL;
}

//main
int main(void)
{
    srand((unsigned)time(NULL));

    for (int iterator = 0; iterator < NUMBER_OF_ACCOUNTS; ++iterator)
    {
        accounts[iterator] = INITIAL_BALANCE;
        pthread_mutex_init(&record_lock[iterator], NULL);
    }

    int initial_sum = total_balance();

    pthread_t threads[NUMBER_OF_THREADS];

    for (int iterator = 0; iterator < NUMBER_OF_THREADS; ++iterator)
    {
        pthread_create(&threads[iterator], NULL, worker, (void*)(intptr_t)iterator);
    }

    for (int iterator = 0; iterator < NUMBER_OF_THREADS; ++iterator)
    {
        pthread_join(threads[iterator], NULL);
    }

    int final_sum = total_balance();

    printf("Initial total = Rs. %d, Final total = Rs. %d \n", initial_sum, final_sum);

    for (int iterator = 0; iterator < NUMBER_OF_ACCOUNTS; iterator++)
    {
        printf("Account [%d] = %d \n", iterator, accounts[iterator]);
    }

    return 0;
}