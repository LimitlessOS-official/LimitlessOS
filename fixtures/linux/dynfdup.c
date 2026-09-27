typedef unsigned long size_t;
typedef long ssize_t;

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define FD_CLOEXEC 1
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2

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
    long duplicate_flags;
    long bytes;
    int fd;
    int duplicate_fd;

    fd = openat(AT_FDCWD, "/nvme/apps/data/file.txt", O_RDONLY, 0);
    if (fd < 0)
    {
        write(1, "dynfdup:openat-fail\n", 20);
        return 1;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        write(1, "dynfdup:setfd-fail\n", 19);
        return 2;
    }

    duplicate_fd = (int)fcntl(fd, F_DUPFD, 7);
    if (duplicate_fd < 7)
    {
        write(1, "dynfdup:dupfd-fail\n", 19);
        return 3;
    }

    duplicate_flags = fcntl(duplicate_fd, F_GETFD, 0);
    if ((duplicate_flags < 0) || ((duplicate_flags & FD_CLOEXEC) != 0))
    {
        write(1, "dynfdup:cloexec-leak\n", 21);
        return 4;
    }

    bytes = read(duplicate_fd, buffer, sizeof(buffer));
    if ((bytes <= 0) || (text_has_nested_fat32(buffer, bytes) == 0))
    {
        write(1, "dynfdup:read-fail\n", 18);
        return 5;
    }

    if ((close(duplicate_fd) != 0) || (close(fd) != 0))
    {
        write(1, "dynfdup:close-fail\n", 19);
        return 6;
    }

    write(1, "dynfdup:dupfd:no-cloexec\n", 25);
    return 0;
}
