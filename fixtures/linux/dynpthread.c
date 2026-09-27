#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_value = 0;

static void *worker(void *arg)
{
    int *value = (int *)arg;

    if (pthread_mutex_lock(&g_mutex) != 0)
    {
        return (void *)2;
    }
    g_value = *value + 1;
    if (pthread_mutex_unlock(&g_mutex) != 0)
    {
        return (void *)3;
    }

    return (void *)0;
}

int main(int argc, char **argv)
{
    pthread_t thread;
    int input = 41;

    if (argc != 1) return 4;
    if ((argv == 0) || (argv[0] == 0)) return 5;

    if (pthread_mutex_lock(&g_mutex) != 0) return 6;
    g_value = 1;
    if (pthread_mutex_unlock(&g_mutex) != 0) return 7;

    if (pthread_create(&thread, 0, worker, &input) != 0) return 8;
    if (pthread_join(thread, 0) != 0) return 9;

    if (pthread_mutex_lock(&g_mutex) != 0) return 11;
    if (g_value != 42) return 12;
    if (pthread_mutex_unlock(&g_mutex) != 0) return 13;

    if (puts("dynpthread-pass") < 0) return 14;
    return 0;
}
