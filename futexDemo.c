#define _GNU_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<unistd.h>
#include<pthread.h>
#include<linux/futex.h>
#include<sys/syscall.h>
#include<sys/time.h>
#include<stdatomic.h>

//wrappers
static int futex_wait(atomic_int *address, int expected)
{
    //FUTEX_WAIT | FUTEX_PRIVATE_FLAG helps to keep the futex local to the process
    return syscall(SYS_futex, (int *)address, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expected, NULL, NULL, 0);
}

static int futex_wake(atomic_int *address, int number)
{
    return syscall(SYS_futex, (int *)address, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, number, NULL, NULL, 0);
}

//signaler()
void *signaler(void *arg)
{
    atomic_int *futex_arguement = arg;
    printf("[Signaler] Sleeping 1s then setting the futex to 1 \n");
    sleep(1);
    atomic_store_explicit(futex_arguement, 1, memory_order_seq_cst);

    // wake up one waiter
    int woken = futex_wake(futex_arguement, 1);

    if(woken >= 0)
    {
        printf("[Signaler] Futex set to 1, futex_wake returned  %d \n", woken);
    }
    else
    {
        perror("fitex_wake");
    }

    return NULL;
}

//waiter()
void *waiter(void *arg)
{
    atomic_int *futex_arguement = arg;
    printf("[Waiter] Startup, checking futex... \n");

    //faster userspace loop, calls futex_wait, only when the value is zero
    while(1)
    {
        int value = atomic_load_explicit(futex_arguement, memory_order_seq_cst);
        if (value != 0)
        {
            break;
        }

        int returned_value = futex_wait(futex_arguement, 0);

        if(returned_value == -1 && errno == EAGAIN)
        {
            continue;
        }
        else if( returned_value == -1 && errno == EINTR)
        {
            continue;
        }
        else if(returned_value == -1)
        {
            perror("futex_wait");
            return NULL;
        }
    }

    printf("[Waiter] Observed futex != 0, proceeding further..\n");
    return NULL;
}

//main()
int main(void)
{
    pthread_t t_waiter, t_signaler;
    atomic_int futex_value;
    atomic_init(&futex_value, 0);

    if(pthread_create(&t_waiter, NULL, waiter, &futex_value) != 0)
    {
        perror("pthread_create waiter");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&t_signaler, NULL, signaler, &futex_value) != 0)
    {
        perror("pthread_create signaler");
        exit(EXIT_FAILURE);
    }

    pthread_join(t_waiter, NULL);
    pthread_join(t_signaler, NULL);

    printf("Futex execution done, final value = %d \n", atomic_load_explicit(&futex_value, memory_order_seq_cst));
    return 0;
}
