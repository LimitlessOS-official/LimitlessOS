#define O_RDONLY 0
#define POLLIN 0x0001
#define POLLOUT 0x0004

typedef unsigned long size_t;
typedef long ssize_t;

struct iovec
{
    void *iov_base;
    size_t iov_len;
};

struct pollfd
{
    int fd;
    short events;
    short revents;
};

extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
extern ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
extern int poll(struct pollfd *fds, unsigned long nfds, int timeout);

static int has_prefix(const char *value, const char *prefix)
{
    unsigned long index;

    index = 0ul;
    while (prefix[index] != '\0')
    {
        if (value[index] != prefix[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int write_text(const char *text, unsigned long length)
{
    struct iovec iov;

    iov.iov_base = (void *)text;
    iov.iov_len = length;
    return writev(1, &iov, 1) == (ssize_t)length ? 0 : -1;
}

int main(void)
{
    char first[8];
    char second[22];
    struct iovec read_iov[2];
    struct iovec write_iov[5];
    struct pollfd fds[1];
    int fd;
    int ready;
    ssize_t got;
    ssize_t wrote;

    fd = open("/nvme/apps/data/file.txt", O_RDONLY);
    if (fd < 0)
    {
        (void)write_text("dynvec:open-fail\n", 17ul);
        return 1;
    }

    fds[0].fd = 1;
    fds[0].events = POLLOUT;
    fds[0].revents = 0;
    ready = poll(fds, 1ul, 0);
    if ((ready != 1) || ((fds[0].revents & POLLOUT) == 0))
    {
        (void)close(fd);
        (void)write_text("dynvec:poll-fail\n", 17ul);
        return 2;
    }

    read_iov[0].iov_base = first;
    read_iov[0].iov_len = 6ul;
    read_iov[1].iov_base = second;
    read_iov[1].iov_len = 21ul;
    got = readv(fd, read_iov, 2);
    (void)close(fd);
    if (got != 27)
    {
        (void)write_text("dynvec:readv-fail\n", 18ul);
        return 3;
    }

    first[6] = '\0';
    second[21] = '\0';
    if ((has_prefix(first, "Nested") == 0) || (has_prefix(second, " FAT32 path fixture") == 0))
    {
        (void)write_text("dynvec:content-fail\n", 20ul);
        return 4;
    }

    write_iov[0].iov_base = (void *)"dynvec:";
    write_iov[0].iov_len = 7ul;
    write_iov[1].iov_base = first;
    write_iov[1].iov_len = 6ul;
    write_iov[2].iov_base = (void *)":";
    write_iov[2].iov_len = 1ul;
    write_iov[3].iov_base = second + 1;
    write_iov[3].iov_len = 18ul;
    write_iov[4].iov_base = (void *)"\n";
    write_iov[4].iov_len = 1ul;
    wrote = writev(1, write_iov, 5);
    if (wrote != 33)
    {
        return 5;
    }

    return 0;
}
