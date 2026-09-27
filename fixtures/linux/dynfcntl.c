typedef unsigned long size_t;
typedef long ssize_t;

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_NONBLOCK 0x800
#define FD_CLOEXEC 1
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

extern int openat(int dirfd, const char *path, int flags, ...);
extern long fcntl(int fd, int command, ...);
extern ssize_t read(int fd, void *buffer, size_t count);
extern ssize_t write(int fd, const void *buffer, size_t count);
extern int close(int fd);

static int text_has_nested_fat32(const char *buffer, long count)
{
    long index;
    int saw_nested = 0;
    int saw_fat32 = 0;

    for (index = 0; index + 6 <= count; ++index)
    {
        if ((buffer[index] == 'N')
            && (buffer[index + 1] == 'e')
            && (buffer[index + 2] == 's')
            && (buffer[index + 3] == 't')
            && (buffer[index + 4] == 'e')
            && (buffer[index + 5] == 'd'))
        {
            saw_nested = 1;
        }
    }

    for (index = 0; index + 5 <= count; ++index)
    {
        if ((buffer[index] == 'F')
            && (buffer[index + 1] == 'A')
            && (buffer[index + 2] == 'T')
            && (buffer[index + 3] == '3')
            && (buffer[index + 4] == '2'))
        {
            saw_fat32 = 1;
        }
    }

    return (saw_nested != 0) && (saw_fat32 != 0);
}

int main(void)
{
    char buffer[64];
    long bytes;
    long fd_flags_before;
    long fd_flags_after;
    long status_flags_before;
    long status_flags_after;
    int fd;

    fd = openat(AT_FDCWD, "/nvme/apps/data/file.txt", O_RDONLY, 0);
    if (fd < 0)
    {
        write(1, "dynfcntl:openat-fail\n", 21);
        return 1;
    }

    fd_flags_before = fcntl(fd, F_GETFD, 0);
    if (fd_flags_before != 0)
    {
        write(1, "dynfcntl:getfd-before-fail\n", 27);
        return 2;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        write(1, "dynfcntl:setfd-fail\n", 20);
        return 3;
    }

    fd_flags_after = fcntl(fd, F_GETFD, 0);
    if ((fd_flags_after & FD_CLOEXEC) == 0)
    {
        write(1, "dynfcntl:getfd-after-fail\n", 26);
        return 4;
    }

    status_flags_before = fcntl(fd, F_GETFL, 0);
    if ((status_flags_before & O_NONBLOCK) != 0)
    {
        write(1, "dynfcntl:getfl-before-fail\n", 27);
        return 5;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0)
    {
        write(1, "dynfcntl:setfl-fail\n", 20);
        return 6;
    }

    status_flags_after = fcntl(fd, F_GETFL, 0);
    if ((status_flags_after & O_NONBLOCK) == 0)
    {
        write(1, "dynfcntl:getfl-after-fail\n", 26);
        return 7;
    }

    bytes = read(fd, buffer, sizeof(buffer));
    if ((bytes <= 0) || (text_has_nested_fat32(buffer, bytes) == 0))
    {
        write(1, "dynfcntl:read-fail\n", 19);
        return 8;
    }

    close(fd);
    write(1, "dynfcntl:cloexec:nonblock\n", 26);
    return 0;
}
