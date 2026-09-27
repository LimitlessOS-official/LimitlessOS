extern int pipe(int pipefd[2]);
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
    char buffer[8];
    long written;
    long got;

    if (pipe(fds) != 0)
    {
        (void)write(1, "dynpipe:pipe-fail\n", 18);
        return 2;
    }

    written = write(fds[1], "hello", 5);
    if (written != 5)
    {
        (void)close(fds[0]);
        (void)close(fds[1]);
        (void)write(1, "dynpipe:write-fail\n", 19);
        return 3;
    }

    (void)close(fds[1]);
    got = read(fds[0], buffer, sizeof(buffer));
    (void)close(fds[0]);

    if ((got != 5) || (text_eq(buffer, "hello", 5) == 0))
    {
        (void)write(1, "dynpipe:read-fail\n", 18);
        return 4;
    }

    (void)write(1, "dynpipe:hello\n", 14);
    return 0;
}
