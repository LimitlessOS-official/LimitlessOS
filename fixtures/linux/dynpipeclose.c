extern int pipe(int pipefd[2]);
extern int fork(void);
extern long wait4(long pid, int *status, int options, void *rusage);
extern long read(int fd, void *buf, unsigned long count);
extern long write(int fd, const void *buf, unsigned long count);
extern int close(int fd);

#define DYNPIPECLOSE_SIGPIPE 13L
#define DYNPIPECLOSE_EPIPE 32L
#define DYNPIPECLOSE_SYS_RT_SIGACTION 13L
#define DYNPIPECLOSE_SYS_RT_SIGRETURN 15L
#define DYNPIPECLOSE_SYS_EXIT_GROUP 231L
#define DYNPIPECLOSE_SA_RESTORER 0x04000000UL

typedef unsigned long dynpipeclose_u64;

struct dynpipeclose_sigaction
{
    void (*handler)(int);
    dynpipeclose_u64 flags;
    void (*restorer)(void);
    dynpipeclose_u64 mask;
};

static volatile int g_dynpipeclose_sigpipe_seen = 0;

static unsigned long dynpipeclose_len(const char *text)
{
    unsigned long length = 0;

    while (text[length] != 0)
    {
        ++length;
    }

    return length;
}

static void dynpipeclose_print(const char *text)
{
    (void)write(1, text, dynpipeclose_len(text));
}

static long dynpipeclose_syscall4(long nr, long a0, long a1, long a2, long a3)
{
    register long r10 __asm__("r10") = a3;
    long ret;

    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
        : "rcx", "r11", "memory");

    return ret;
}

static void dynpipeclose_exit_group(int code)
{
    (void)dynpipeclose_syscall4(DYNPIPECLOSE_SYS_EXIT_GROUP, (long)code, 0, 0, 0);
    for (;;)
    {
    }
}

__attribute__((naked)) static void dynpipeclose_rt_sigreturn(void)
{
    __asm__(
        "mov $15, %rax\n"
        "syscall\n");
}

static void dynpipeclose_sigpipe_handler(int signal_number)
{
    g_dynpipeclose_sigpipe_seen = signal_number;
    dynpipeclose_print("sigpipe-caught\n");
}

static int dynpipeclose_install_sigpipe(void)
{
    struct dynpipeclose_sigaction action;
    long result;

    action.handler = dynpipeclose_sigpipe_handler;
    action.flags = DYNPIPECLOSE_SA_RESTORER;
    action.restorer = dynpipeclose_rt_sigreturn;
    action.mask = 0;

    result = dynpipeclose_syscall4(
        DYNPIPECLOSE_SYS_RT_SIGACTION,
        DYNPIPECLOSE_SIGPIPE,
        (long)&action,
        0,
        8);

    return (result == 0) ? 0 : -1;
}

static int dynpipeclose_expect_blocked_eof(void)
{
    int fds[2];
    int status;
    int child;
    char value = 0x55;
    long got;
    long waited;

    if (pipe(fds) != 0)
    {
        dynpipeclose_print("dynpipeclose:eof-pipe-fail\n");
        return 0;
    }

    child = fork();
    if (child < 0)
    {
        (void)close(fds[0]);
        (void)close(fds[1]);
        dynpipeclose_print("dynpipeclose:eof-fork-fail\n");
        return 0;
    }

    if (child == 0)
    {
        (void)close(fds[0]);
        (void)close(fds[1]);
        dynpipeclose_exit_group(7);
    }

    (void)close(fds[1]);
    got = read(fds[0], &value, 1);
    (void)close(fds[0]);

    status = 0;
    waited = wait4((long)child, &status, 0, (void *)0);
    if ((waited != (long)child) || (((unsigned int)status >> 8) != 7u))
    {
        dynpipeclose_print("dynpipeclose:eof-wait-fail\n");
        return 0;
    }

    if ((got != 0) || (value != 0x55))
    {
        dynpipeclose_print("dynpipeclose:eof-read-fail\n");
        return 0;
    }

    dynpipeclose_print("dynpipeclose:eof\n");
    return 1;
}

static int dynpipeclose_expect_sigpipe(void)
{
    int fds[2];
    long written;

    if (dynpipeclose_install_sigpipe() != 0)
    {
        dynpipeclose_print("dynpipeclose:sigaction-fail\n");
        return 0;
    }

    if (pipe(fds) != 0)
    {
        dynpipeclose_print("dynpipeclose:sigpipe-pipe-fail\n");
        return 0;
    }

    (void)close(fds[0]);
    written = write(fds[1], "x", 1);
    (void)close(fds[1]);

    if ((written != -DYNPIPECLOSE_EPIPE)
        || (g_dynpipeclose_sigpipe_seen != (int)DYNPIPECLOSE_SIGPIPE))
    {
        dynpipeclose_print("dynpipeclose:sigpipe-fail\n");
        return 0;
    }

    return 1;
}

int main(void)
{
    if (dynpipeclose_expect_blocked_eof() == 0)
    {
        return 2;
    }

    if (dynpipeclose_expect_sigpipe() == 0)
    {
        return 3;
    }

    dynpipeclose_print("dynpipeclose:done\n");
    return 0;
}
