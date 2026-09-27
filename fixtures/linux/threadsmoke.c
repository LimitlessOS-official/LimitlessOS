#include <pthread.h>
#include <unistd.h>

static volatile int g_shared_slots[2];

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

static void *thread_main(void *arg)
{
    long index = (long)arg;
    int local = 0;

    for (int i = 0; i < 1000; ++i) {
        local += 1;
    }

    if ((index >= 0) && (index < 2)) {
        g_shared_slots[index] = local;
    }

    return 0;
}

int main(void)
{
    pthread_t threads[2];
    int rc0;
    int rc1;

    rc0 = pthread_create(&threads[0], 0, thread_main, (void *)0);
    rc1 = pthread_create(&threads[1], 0, thread_main, (void *)1);
    if ((rc0 != 0) || (rc1 != 0)) {
        const char prefix[] = "threadsmoke-create-failed rc0=";
        const char middle[] = " rc1=";
        const char suffix[] = "\n";
        (void)write(1, prefix, sizeof(prefix) - 1);
        write_u32((unsigned int)rc0);
        (void)write(1, middle, sizeof(middle) - 1);
        write_u32((unsigned int)rc1);
        (void)write(1, suffix, sizeof(suffix) - 1);
        return 2;
    }

    (void)pthread_join(threads[0], 0);
    (void)pthread_join(threads[1], 0);

    if ((g_shared_slots[0] == 1000) && (g_shared_slots[1] == 1000)) {
        const char ok[] = "threadsmoke-done\n";
        (void)write(1, ok, sizeof(ok) - 1);
        return 0;
    }

    const char failed[] = "threadsmoke-shared-failed\n";
    (void)write(1, failed, sizeof(failed) - 1);
    return 3;
}
