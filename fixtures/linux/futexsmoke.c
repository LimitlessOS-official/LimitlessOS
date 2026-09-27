#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_worker1_ready;
static volatile int g_shared_count;

static void write_literal(const char *text, unsigned long bytes)
{
    (void)write(1, text, bytes);
}

#define WRITE_LITERAL(text) write_literal((text), sizeof(text) - 1)

static void write_u32(unsigned int value)
{
    char digits[10];
    unsigned int index = 0;

    if (value == 0) {
        const char zero[] = "0";
        (void)write(1, zero, sizeof(zero) - 1);
        return;
    }

    while ((value != 0) && (index < sizeof(digits))) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index != 0) {
        --index;
        (void)write(1, &digits[index], 1);
    }
}

static long futex_wait_word(volatile int *address, int expected)
{
    return syscall(202, address, FUTEX_WAIT_PRIVATE, expected, 0, 0, 0);
}

static long futex_wake_word(volatile int *address)
{
    return syscall(202, address, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
}

static void *worker_main(void *arg)
{
    long index = (long)arg;

    if (index == 0) {
        WRITE_LITERAL("futexsmoke-unlocker-wait\n");
        while (g_worker1_ready == 0) {
            (void)futex_wait_word(&g_worker1_ready, 0);
        }
        WRITE_LITERAL("futexsmoke-unlocker-unlock\n");
        (void)pthread_mutex_unlock(&g_mutex);
        WRITE_LITERAL("futexsmoke-unlocker-done\n");
        return 0;
    }

    g_worker1_ready = 1;
    (void)futex_wake_word(&g_worker1_ready);
    WRITE_LITERAL("futexsmoke-worker1-lock\n");
    if (pthread_mutex_lock(&g_mutex) != 0) {
        return (void *)1;
    }
    WRITE_LITERAL("futexsmoke-worker1-locked\n");
    __sync_fetch_and_add(&g_shared_count, 1);
    (void)pthread_mutex_unlock(&g_mutex);
    WRITE_LITERAL("futexsmoke-worker1-unlocked\n");
    return 0;
}

int main(void)
{
    pthread_t threads[2];
    void *result0 = (void *)1;
    void *result1 = (void *)1;
    int rc0;
    int rc1;

    WRITE_LITERAL("futexsmoke-start\n");
    if (pthread_mutex_lock(&g_mutex) != 0) {
        WRITE_LITERAL("futexsmoke-main-lock-failed\n");
        return 2;
    }
    WRITE_LITERAL("futexsmoke-main-locked\n");

    rc0 = pthread_create(&threads[0], 0, worker_main, (void *)0);
    WRITE_LITERAL("futexsmoke-created-0\n");
    rc1 = pthread_create(&threads[1], 0, worker_main, (void *)1);
    WRITE_LITERAL("futexsmoke-created-1\n");
    if ((rc0 != 0) || (rc1 != 0)) {
        const char prefix[] = "futexsmoke-create-failed rc0=";
        const char middle[] = " rc1=";
        const char suffix[] = "\n";
        (void)write(1, prefix, sizeof(prefix) - 1);
        write_u32((unsigned int)rc0);
        (void)write(1, middle, sizeof(middle) - 1);
        write_u32((unsigned int)rc1);
        (void)write(1, suffix, sizeof(suffix) - 1);
        return 3;
    }

    WRITE_LITERAL("futexsmoke-join-1\n");
    (void)pthread_join(threads[1], &result1);
    WRITE_LITERAL("futexsmoke-join-0\n");
    (void)pthread_join(threads[0], &result0);

    if ((result0 == 0) && (result1 == 0) && (g_shared_count == 1)) {
        const char ok[] = "futexsmoke-done\n";
        (void)write(1, ok, sizeof(ok) - 1);
        return 0;
    }

    const char failed[] = "futexsmoke-failed count=";
    const char suffix[] = "\n";
    (void)write(1, failed, sizeof(failed) - 1);
    write_u32((unsigned int)g_shared_count);
    (void)write(1, suffix, sizeof(suffix) - 1);
    return 4;
}
