extern int pipe(int pipefd[2]);
extern int fork(void);
extern long wait4(long pid, int *status, int options, void *rusage);
extern long read(int fd, void *buf, unsigned long count);
extern long write(int fd, const void *buf, unsigned long count);
extern int close(int fd);

static int text_eq(const char *left, const char *right, unsigned long count)
{
    unsigned long index;

    for (index = 0; index < count; ++index)
    {
        if (left[index] != right[index])
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    int fds[2];
    int status;
    int child;
    char buffer[16];
    long got;
    long waited;

    if (pipe(fds) != 0)
    {
        (void)write(1, "dynforkpipe:pipe-fail\n", 22);
        return 2;
    }

    child = fork();
    if (child < 0)
    {
        (void)close(fds[0]);
        (void)close(fds[1]);
        (void)write(1, "dynforkpipe:fork-fail\n", 22);
        return 3;
    }

    if (child == 0)
    {
        (void)close(fds[0]);
        if (write(fds[1], "child-pipe", 10) != 10)
        {
            (void)close(fds[1]);
            return 4;
        }
        (void)close(fds[1]);
        return 7;
    }

    (void)close(fds[1]);
    got = read(fds[0], buffer, sizeof(buffer));
    (void)close(fds[0]);

    if ((got != 10) || (text_eq(buffer, "child-pipe", 10) == 0))
    {
        (void)write(1, "dynforkpipe:read-fail\n", 22);
        return 5;
    }

    status = 0;
    waited = wait4((long)child, &status, 0, (void *)0);
    if ((waited != (long)child) || (((unsigned int)status >> 8) != 7u))
    {
        (void)write(1, "dynforkpipe:wait-fail\n", 22);
        return 6;
    }

    (void)write(1, "dynforkpipe:child-pipe\n", 23);
    return 0;
}
