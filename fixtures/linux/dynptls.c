#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#define WORKER_COUNT 4

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static pthread_key_t g_key;
static int g_ready = 0;
static int g_sum = 0;
static int g_bad = 0;
static int g_values[WORKER_COUNT] = { 11, 22, 33, 44 };

static void *worker(void *arg)
{
    int index = (int)(intptr_t)arg;
    int value;
    void *tls_value;

    if ((index < 0) || (index >= WORKER_COUNT))
    {
        return (void *)101;
    }

    value = g_values[index];
    if (pthread_setspecific(g_key, (void *)(intptr_t)value) != 0)
    {
        return (void *)102;
    }

    if (pthread_mutex_lock(&g_mutex) != 0)
    {
        return (void *)103;
    }

    tls_value = pthread_getspecific(g_key);
    if ((int)(intptr_t)tls_value != value)
    {
        g_bad = 1;
    }

    g_sum += value;
    ++g_ready;
    if (pthread_cond_signal(&g_cond) != 0)
    {
        g_bad = 1;
    }

    if (pthread_mutex_unlock(&g_mutex) != 0)
    {
        return (void *)104;
    }

    return (void *)0;
}

int main(void)
{
    pthread_t threads[WORKER_COUNT];
    int index;
    int create_count = 0;

    if (pthread_key_create(&g_key, 0) != 0)
    {
        return 1;
    }

    if (pthread_mutex_lock(&g_mutex) != 0)
    {
        return 2;
    }

    for (index = 0; index < WORKER_COUNT; ++index)
    {
        if (pthread_create(&threads[index], 0, worker, (void *)(intptr_t)index) != 0)
        {
            g_bad = 1;
            break;
        }
        ++create_count;
    }

    while ((g_ready < create_count) && (g_bad == 0))
    {
        if (pthread_cond_wait(&g_cond, &g_mutex) != 0)
        {
            return 3;
        }
    }

    if (pthread_mutex_unlock(&g_mutex) != 0)
    {
        return 6;
    }

    for (index = 0; index < create_count; ++index)
    {
        if (pthread_join(threads[index], 0) != 0)
        {
            return 7;
        }
    }

    if ((create_count != WORKER_COUNT)
        || (g_ready != WORKER_COUNT)
        || (g_sum != 110)
        || (g_bad != 0))
    {
        return 8;
    }

    if (puts("dynptls-pass") < 0)
    {
        return 9;
    }

    return 0;
}
