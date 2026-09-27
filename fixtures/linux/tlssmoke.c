#include <pthread.h>
#include <unistd.h>

#define TLSSMOKE_WORKERS 8u

static __thread int tls_val;
static volatile int g_observed[TLSSMOKE_WORKERS];
static volatile int g_expected[TLSSMOKE_WORKERS] = {
    101, 202, 303, 404, 505, 606, 707, 808
};

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

static void *thread_main(void *arg)
{
    unsigned long index = (unsigned long)arg;
    volatile unsigned int spin;

    tls_val = g_expected[index];
    for (spin = 0u; spin < 1024u; ++spin) {
        (void)getpid();
    }

    g_observed[index] = tls_val;
    WRITE_LITERAL("tlssmoke-thread ");
    write_u32((unsigned int)index);
    WRITE_LITERAL(" observed ");
    write_u32((unsigned int)g_observed[index]);
    WRITE_LITERAL("\n");
    return 0;
}

int main(void)
{
    pthread_t threads[TLSSMOKE_WORKERS];
    void *result = (void *)1;
    unsigned int index;
    unsigned int created = 0u;
    int failed = 0;
    int rc;

    WRITE_LITERAL("tlssmoke-start\n");

    for (index = 0u; index < TLSSMOKE_WORKERS; ++index) {
        rc = pthread_create(&threads[index], 0, thread_main, (void *)(unsigned long)index);
        if (rc != 0) {
            WRITE_LITERAL("tlssmoke-create-failed thread ");
            write_u32(index);
            WRITE_LITERAL(" rc ");
            write_u32((unsigned int)rc);
            WRITE_LITERAL("\n");
            failed = 1;
            break;
        }
        ++created;
    }

    for (index = 0u; index < created; ++index) {
        result = (void *)1;
        if (pthread_join(threads[index], &result) != 0) {
            failed = 1;
        }
        if (result != 0) {
            failed = 1;
        }
    }

    for (index = 0u; index < TLSSMOKE_WORKERS; ++index) {
        if (g_observed[index] != g_expected[index]) {
            WRITE_LITERAL("tlssmoke-mismatch thread ");
            write_u32(index);
            WRITE_LITERAL(" expected ");
            write_u32((unsigned int)g_expected[index]);
            WRITE_LITERAL(" observed ");
            write_u32((unsigned int)g_observed[index]);
            WRITE_LITERAL("\n");
            failed = 1;
        }
    }

    if (failed == 0) {
        WRITE_LITERAL("tlssmoke-done\n");
        return 0;
    }

    WRITE_LITERAL("tlssmoke-failed\n");
    return 2;
}
