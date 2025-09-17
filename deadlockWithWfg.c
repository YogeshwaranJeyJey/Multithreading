#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define MAXIMUM_THREADS 4
#define MAXIMUM_LOCKS 2

typedef struct {
    pthread_mutex_t mutex_value;
    pthread_cond_t  condition_variable; //wait or wake
    pthread_t owner;          // 0 if free
} TrackLock;

static TrackLock track_lock[MAXIMUM_LOCKS];
static pthread_t threads[2];
static int abort_flag[MAXIMUM_THREADS];

// WFG: wait_for[iterator][jterator] = 1 means thread iterator waits for thread jterator 
static int wait_for[MAXIMUM_THREADS][MAXIMUM_THREADS];
static pthread_mutex_t wfg_mutex = PTHREAD_MUTEX_INITIALIZER;

static int equate(pthread_t thread1, pthread_t thread2) 
{ 
    return pthread_equal(thread1, thread2); 
}

// Registry: map pthread_t to a small index 0..(n-1) 
static int index_of(pthread_t thread) {
    for (int iterator = 0; iterator < 2; ++iterator) 
    {
        if (equate(threads[iterator], thread)) 
        {
            return iterator;
        }
    }
    return -1;
}

static void wfg_add_edge(int waiter_idx, int owner_idx) {
    pthread_mutex_lock(&wfg_mutex);
    wait_for[waiter_idx][owner_idx] = 1;
    pthread_mutex_unlock(&wfg_mutex);
}

static void wfg_remove_all_from(int waiter_idx) {
    pthread_mutex_lock(&wfg_mutex);
    for (int iterator = 0; iterator < MAXIMUM_THREADS; ++iterator) 
    {
        wait_for[waiter_idx][iterator] = 0;
    }
    pthread_mutex_unlock(&wfg_mutex);
}

static int detect_cycle_dfs(int u, int *vis, int *stk) {
    vis[u] = 1;
    stk[u] = 1; //marks the node in the stack (recursively)
    for (int v = 0; v < MAXIMUM_THREADS; ++v) {
        if (!wait_for[u][v])
        {
            continue; //skip when there is no edge
        } 
        if (!vis[v] && detect_cycle_dfs(v, vis, stk)) 
        {
            return 1; //Deeper Depth first search
        }
        if (stk[v])
        {
            return 1;   // back-edge → cycle
        } 
    }
    stk[u] = 0;
    return 0;
}

static int has_cycle(void) {
    int vis[MAXIMUM_THREADS] = {0};
    int stk[MAXIMUM_THREADS] = {0};
    for (int i = 0; i < MAXIMUM_THREADS; ++i)
    {
        if (!vis[i] && detect_cycle_dfs(i, vis, stk))
        {
            return 1;
        }
    }
    return 0;
}

static void dump_wfg(void) {
    pthread_mutex_lock(&wfg_mutex);
    printf("WFG:\n");
    for (int i = 0; i < MAXIMUM_THREADS; ++i) {
        for (int j = 0; j < MAXIMUM_THREADS; ++j) printf("%d ", wait_for[i][j]);
        printf("\n");
    }
    pthread_mutex_unlock(&wfg_mutex);
}

// Our tracked lock acquire: blocks and records wait edges 
static int track_acquire(TrackLock *lk) {
    pthread_t self = pthread_self();
    int self_idx = index_of(self);
    pthread_mutex_lock(&lk->mutex_value);
    while (lk->owner && !equate(lk->owner, self)) {
        int owner_idx = index_of(lk->owner);
        if (self_idx >= 0 && owner_idx >= 0) wfg_add_edge(self_idx, owner_idx);
        // wait; a monitor can signal if aborting someone 
        pthread_cond_wait(&lk->condition_variable, &lk->mutex_value);
        if (self_idx >= 0 && abort_flag[self_idx]) {
            // abort: cleanup wait edges and return failure 
            if (self_idx >= 0) wfg_remove_all_from(self_idx);
            pthread_mutex_unlock(&lk->mutex_value);
            return 0;
        }
    }
    lk->owner = self;
    if (self_idx >= 0) wfg_remove_all_from(self_idx);
    pthread_mutex_unlock(&lk->mutex_value);
    return 1;
}

static void track_release(TrackLock *lk) {
    pthread_mutex_lock(&lk->mutex_value);
    if (lk->owner && equate(lk->owner, pthread_self())) {
        lk->owner = (pthread_t)0;
        pthread_cond_broadcast(&lk->condition_variable);
    }
    pthread_mutex_unlock(&lk->mutex_value);
}

// Monitor: periodically checks WFG; if cycle, abort a victim 
static void *monitor_function(void *arg) {
    (void)arg;
    for (;;) {
        usleep(100000); // 100 ms 
        pthread_mutex_lock(&wfg_mutex);
        int cyc = has_cycle();
        pthread_mutex_unlock(&wfg_mutex);
        if (cyc) {
            dump_wfg();
            // choose victim: highest index waiting 
            int victim = 1;
            abort_flag[victim] = 1;
            printf("Deadlock detected. Aborting T%d\n", victim);
            // wake all waiters 
            for (int i = 0; i < MAXIMUM_LOCKS; ++i) {
                pthread_mutex_lock(&track_lock[i].mutex_value);
                pthread_cond_broadcast(&track_lock[i].condition_variable);
                pthread_mutex_unlock(&track_lock[i].mutex_value);
            }
            break; // stop after first resolution for this demo 
        }
    }
    return NULL;
}

// Workers intentionally lock in opposite orders to cause deadlock 
static void *worker(void *arg) {
    int tid = (int)(intptr_t)arg;
    TrackLock *first = (tid == 0) ? &track_lock[0] : &track_lock[1];
    TrackLock *second= (tid == 0) ? &track_lock[1] : &track_lock[0];

    printf("T%d: acquiring first\n", tid);
    if (!track_acquire(first)) { printf("T%d: aborted early\n", tid); return NULL; }
    usleep(200000); // increase chance of overlap 

    printf("T%d: acquiring second (may deadlock)\n", tid);
    if (!track_acquire(second)) {
        printf("T%d: aborted while waiting second. Releasing first.\n", tid);
        track_release(first);
        return NULL;
    }

    printf("T%d: got both. Doing work.\n", tid);
    usleep(100000);
    track_release(second);
    track_release(first);
    printf("T%d: done.\n", tid);
    return NULL;
}

int main(void) {
    for (int i = 0; i < MAXIMUM_LOCKS; ++i) {
        pthread_mutex_init(&track_lock[i].mutex_value, NULL);
        pthread_cond_init(&track_lock[i].condition_variable, NULL);
        track_lock[i].owner = (pthread_t)0;
    }
    abort_flag[0] = abort_flag[1] = 0;

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_function, NULL);
    pthread_create(&threads[0], NULL, worker, (void*)(intptr_t)0);
    pthread_create(&threads[1], NULL, worker, (void*)(intptr_t)1);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(monitor_thread, NULL);
    printf("Demo complete.\n");
    return 0;
}