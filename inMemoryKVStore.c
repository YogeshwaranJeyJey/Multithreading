#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>

#define NUM_KEYS 8
#define THREADS 4
#define TX_OPS_PER_THREAD 100
#define MAX_LOCKS_PER_TX 32
#define MAX_WRITESET_PER_TX 32

typedef struct Version
{
    int val;
    uint64_t commit_ts;
    struct Version *next;
} Version;

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t owner;
} TrackLock;

typedef struct
{
    int locked_keys[MAX_LOCKS_PER_TX];
    int locked_count;
    int write_keys[MAX_WRITESET_PER_TX];
    int write_vals[MAX_WRITESET_PER_TX];
    int write_count;
    uint64_t start_ts;
    int aborted;
} Tx;

static Version *versions[NUM_KEYS];
static pthread_mutex_t versions_mutex[NUM_KEYS];
static int kv_dummy[NUM_KEYS];

static TrackLock key_lock[NUM_KEYS];
static pthread_t workers[THREADS];

static int wait_for[THREADS][THREADS];
static pthread_mutex_t wfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static int abort_flag[THREADS];
static int finished_workers = 0;
static pthread_mutex_t finished_mutex = PTHREAD_MUTEX_INITIALIZER;

static _Atomic uint64_t global_ts = 2;
static int equal_threads(pthread_t a, pthread_t b) { return pthread_equal(a, b); }

static int index_of(pthread_t t)
{
    for (int i = 0; i < THREADS; ++i)
        if (equal_threads(workers[i], t))
            return i;
    return -1;
}

static void wfg_add_edge(int waiter, int owner)
{
    if (waiter < 0 || owner < 0)
        return;
    pthread_mutex_lock(&wfg_mutex);
    wait_for[waiter][owner] = 1;
    pthread_mutex_unlock(&wfg_mutex);
}

static void wfg_remove_all_from(int waiter)
{
    if (waiter < 0)
        return;
    pthread_mutex_lock(&wfg_mutex);
    for (int j = 0; j < THREADS; ++j)
        wait_for[waiter][j] = 0;
    pthread_mutex_unlock(&wfg_mutex);
}

static void dump_wfg(void)
{
    pthread_mutex_lock(&wfg_mutex);
    printf("WFG:\n");
    for (int i = 0; i < THREADS; ++i)
    {
        for (int j = 0; j < THREADS; ++j)
            printf("%d ", wait_for[i][j]);
        printf("\n");
    }
    pthread_mutex_unlock(&wfg_mutex);
}

static int detect_cycle_dfs(int u, int *vis, int *stack)
{
    vis[u] = 1;
    stack[u] = 1;
    for (int v = 0; v < THREADS; ++v)
    {
        if (!wait_for[u][v])
            continue;
        if (!vis[v] && detect_cycle_dfs(v, vis, stack))
            return 1;
        if (stack[v])
            return 1;
    }
    stack[u] = 0;
    return 0;
}

static int has_cycle(void)
{
    int vis[THREADS];
    int stack[THREADS];
    memset(vis, 0, sizeof(vis));
    memset(stack, 0, sizeof(stack));
    for (int i = 0; i < THREADS; ++i)
        if (!vis[i] && detect_cycle_dfs(i, vis, stack))
            return 1;
    return 0;
}

static int key_lock_acquire(TrackLock *lk)
{
    pthread_t self = pthread_self();
    int self_idx = index_of(self);
    pthread_mutex_lock(&lk->mutex);
    while (lk->owner && !equal_threads(lk->owner, self))
    {
        int owner_idx = index_of(lk->owner);
        if (self_idx >= 0 && owner_idx >= 0)
            wfg_add_edge(self_idx, owner_idx);
        pthread_cond_wait(&lk->cond, &lk->mutex);
        if (self_idx >= 0 && abort_flag[self_idx])
        {
            if (self_idx >= 0)
                wfg_remove_all_from(self_idx);
            pthread_mutex_unlock(&lk->mutex);
            return 0;
        }
    }
    lk->owner = self;
    if (self_idx >= 0)
        wfg_remove_all_from(self_idx);
    pthread_mutex_unlock(&lk->mutex);
    return 1;
}

static void key_lock_release(TrackLock *lk)
{
    pthread_mutex_lock(&lk->mutex);
    if (lk->owner && equal_threads(lk->owner, pthread_self()))
    {
        lk->owner = (pthread_t)0;
        pthread_cond_broadcast(&lk->cond);
    }
    pthread_mutex_unlock(&lk->mutex);
}

static uint64_t next_ts(void)
{
    return (uint64_t)atomic_fetch_add(&global_ts, 1);
}

static void version_append(int key, int val, uint64_t commit_ts)
{
    Version *v = malloc(sizeof(Version));
    v->val = val;
    v->commit_ts = commit_ts;
    pthread_mutex_lock(&versions_mutex[key]);
    v->next = versions[key];
    versions[key] = v;
    pthread_mutex_unlock(&versions_mutex[key]);
}

static int version_read_snapshot(int key, uint64_t snapshot_ts, int *out_val)
{
    pthread_mutex_lock(&versions_mutex[key]);
    Version *cur = versions[key];
    while (cur)
    {
        if (cur->commit_ts <= snapshot_ts)
        {
            *out_val = cur->val;
            pthread_mutex_unlock(&versions_mutex[key]);
            return 1;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&versions_mutex[key]);
    return 0;
}

static void tx_begin(Tx *tx)
{
    tx->locked_count = 0;
    tx->write_count = 0;
    tx->aborted = 0;
    tx->start_ts = atomic_load(&global_ts);
}

static void tx_release_all(Tx *tx)
{
    for (int i = tx->locked_count - 1; i >= 0; --i)
    {
        int k = tx->locked_keys[i];
        key_lock_release(&key_lock[k]);
    }
    tx->locked_count = 0;
}

static int tx_lock_key_for_commit(Tx *tx, int key)
{
    if (tx->locked_count >= MAX_LOCKS_PER_TX)
        return 0;
    for (int i = 0; i < tx->locked_count; ++i)
        if (tx->locked_keys[i] == key)
            return 1;
    if (!key_lock_acquire(&key_lock[key]))
    {
        tx->aborted = 1;
        return 0;
    }
    tx->locked_keys[tx->locked_count++] = key;
    return 1;
}

static int tx_read(Tx *tx, int key, int *out_val)
{
    for (int i = 0; i < tx->write_count; ++i)
    {
        if (tx->write_keys[i] == key)
        {
            *out_val = tx->write_vals[i];
            return 1;
        }
    }
    return version_read_snapshot(key, tx->start_ts, out_val);
}

static int tx_write(Tx *tx, int key, int val)
{
    if (tx->write_count < MAX_WRITESET_PER_TX)
    {
        for (int i = 0; i < tx->write_count; ++i)
        {
            if (tx->write_keys[i] == key)
            {
                tx->write_vals[i] = val;
                return 1;
            }
        }
        tx->write_keys[tx->write_count] = key;
        tx->write_vals[tx->write_count] = val;
        tx->write_count++;
        return 1;
    }
    return 0;
}

static int tx_commit_mvcc(Tx *tx)
{
    if (tx->aborted)
    {
        tx_release_all(tx);
        return 0;
    }
    int order[MAX_WRITESET_PER_TX];
    for (int i = 0; i < tx->write_count; ++i)
        order[i] = tx->write_keys[i];
    for (int i = 0; i < tx->write_count; ++i)
        for (int j = i + 1; j < tx->write_count; ++j)
            if (order[i] > order[j])
            {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }

    for (int i = 0; i < tx->write_count; ++i)
    {
        if (!tx_lock_key_for_commit(tx, order[i]))
        {
            tx_release_all(tx);
            return 0;
        }
    }

    for (int i = 0; i < tx->write_count; ++i)
    {
        int k = tx->write_keys[i];
        pthread_mutex_lock(&versions_mutex[k]);
        Version *latest = versions[k];
        uint64_t latest_ts = latest ? latest->commit_ts : 0;
        pthread_mutex_unlock(&versions_mutex[k]);
        if (latest_ts > tx->start_ts)
        {
            tx->aborted = 1;
            tx_release_all(tx);
            return 0;
        }
    }

    uint64_t commit_ts = next_ts();
    for (int i = 0; i < tx->write_count; ++i)
    {
        int k = tx->write_keys[i];
        int v = tx->write_vals[i];
        version_append(k, v, commit_ts);
    }

    tx_release_all(tx);
    return 1;
}

static void tx_abort(Tx *tx)
{
    tx->aborted = 1;
    tx_release_all(tx);
}

static int detect_cycle_simple(void)
{
    return has_cycle();
}

static void *monitor_thread(void *arg)
{
    (void)arg;
    for (;;)
    {
        usleep(100 * 1000);
        pthread_mutex_lock(&wfg_mutex);
        int cyc = detect_cycle_simple();
        pthread_mutex_unlock(&wfg_mutex);
        if (cyc)
        {
            dump_wfg();
            int victim = -1;
            pthread_mutex_lock(&wfg_mutex);
            for (int i = THREADS - 1; i >= 0; --i)
            {
                int waits = 0;
                for (int j = 0; j < THREADS; ++j)
                    if (wait_for[i][j])
                    {
                        waits = 1;
                        break;
                    }
                if (waits)
                {
                    victim = i;
                    break;
                }
            }
            pthread_mutex_unlock(&wfg_mutex);
            if (victim >= 0)
            {
                abort_flag[victim] = 1;
                for (int k = 0; k < NUM_KEYS; ++k)
                {
                    pthread_mutex_lock(&key_lock[k].mutex);
                    pthread_cond_broadcast(&key_lock[k].cond);
                    pthread_mutex_unlock(&key_lock[k].mutex);
                }
            }
        }
        pthread_mutex_lock(&finished_mutex);
        int done = finished_workers;
        pthread_mutex_unlock(&finished_mutex);
        if (done >= THREADS)
            break;
    }
    return NULL;
}

static void *worker_fn(void *arg)
{
    int tid = (int)(intptr_t)arg;
    srand((unsigned)time(NULL) ^ (tid << 8));
    Tx tx;
    workers[tid] = pthread_self();

    for (int t = 0; t < TX_OPS_PER_THREAD; ++t)
    {
        if (abort_flag[tid])
            break;
        tx_begin(&tx);
        int k1 = rand() % NUM_KEYS;
        int k2 = rand() % NUM_KEYS;
        if (k1 == k2)
            k2 = (k1 + 1) % NUM_KEYS;

        int v1 = 0, v2 = 0;
        if (!tx_read(&tx, k1, &v1))
        {
            tx_abort(&tx);
            continue;
        }
        if (!tx_read(&tx, k2, &v2))
        {
            tx_abort(&tx);
            continue;
        }

        if (!tx_write(&tx, k1, v2) || !tx_write(&tx, k2, v1))
        {
            tx_abort(&tx);
            continue;
        }
        int ok = tx_commit_mvcc(&tx);
        if (!ok)
        {
        }
        usleep((rand() % 100) * 1000);
    }
    pthread_mutex_lock(&finished_mutex);
    finished_workers++;
    pthread_mutex_unlock(&finished_mutex);
    return NULL;
}

int main(void)
{
    for (int i = 0; i < NUM_KEYS; ++i)
    {
        versions[i] = NULL;
        pthread_mutex_init(&versions_mutex[i], NULL);
    }
    for (int i = 0; i < NUM_KEYS; ++i)
        version_append(i, 100 + i, 1);

    for (int i = 0; i < NUM_KEYS; ++i)
    {
        pthread_mutex_init(&key_lock[i].mutex, NULL);
        pthread_cond_init(&key_lock[i].cond, NULL);
        key_lock[i].owner = (pthread_t)0;
    }
    memset(wait_for, 0, sizeof(wait_for));
    memset(abort_flag, 0, sizeof(abort_flag));
    for (int i = 0; i < THREADS; ++i)
        workers[i] = (pthread_t)0;
    pthread_t th[THREADS];
    for (int i = 0; i < THREADS; ++i)
        pthread_create(&th[i], NULL, worker_fn, (void *)(intptr_t)i);
    usleep(50 * 1000);
    pthread_t monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    for (int i = 0; i < THREADS; ++i)
        pthread_join(th[i], NULL);
    pthread_join(monitor, NULL);
    printf("Final MVCC state (latest versions):\n");
    for (int k = 0; k < NUM_KEYS; ++k)
    {
        pthread_mutex_lock(&versions_mutex[k]);
        Version *v = versions[k];
        if (v)
            printf("key[%d] = %d (ts=%" PRIu64 ")\n", k, v->val, v->commit_ts);
        else
            printf("key[%d] = <empty>\n", k);
        pthread_mutex_unlock(&versions_mutex[k]);
    }
    return 0;
}