typedef unsigned long size_t;
typedef long ssize_t;

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_CLOEXEC 0x80000
#define FD_CLOEXEC 1
#define F_GETFD 1

extern int openat(int dirfd, const char *path, int flags, ...);
extern int dup(int oldfd);
extern int dup2(int oldfd, int newfd);
extern int dup3(int oldfd, int newfd, int flags);
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

static int read_nested_file_from_fd(int fd)
{
    char buffer[64];
    long bytes;

    bytes = read(fd, buffer, sizeof(buffer));
    return ((bytes > 0) && (text_has_nested_fat32(buffer, bytes) != 0)) ? 1 : 0;
}

static int open_fixture(void)
{
    return openat(AT_FDCWD, "/nvme/apps/data/file.txt", O_RDONLY, 0);
}

int main(void)
{
    int stdout_dup;
    int fd;
    int duplicated;
    long flags;

    stdout_dup = dup(1);
    if (stdout_dup < 3)
    {
        write(1, "dyndup:stdout-dup-fail\n", 23);
        return 1;
    }

    fd = open_fixture();
    if (fd < 0)
    {
        write(1, "dyndup:open-dup-fail\n", 21);
        return 2;
    }
    duplicated = dup(fd);
    if (duplicated < 3)
    {
        write(1, "dyndup:dup-fail\n", 16);
        return 3;
    }
    if (read_nested_file_from_fd(duplicated) == 0)
    {
        write(1, "dyndup:dup-read-fail\n", 21);
        return 4;
    }
    if ((close(duplicated) != 0) || (close(fd) != 0))
    {
        write(1, "dyndup:dup-close-fail\n", 22);
        return 5;
    }

    fd = open_fixture();
    if (fd < 0)
    {
        write(1, "dyndup:open-dup2-fail\n", 22);
        return 6;
    }
    duplicated = dup2(fd, 7);
    if (duplicated != 7)
    {
        write(1, "dyndup:dup2-fail\n", 17);
        return 7;
    }
    if (read_nested_file_from_fd(duplicated) == 0)
    {
        write(1, "dyndup:dup2-read-fail\n", 22);
        return 8;
    }
    if ((close(duplicated) != 0) || (close(fd) != 0))
    {
        write(1, "dyndup:dup2-close-fail\n", 23);
        return 9;
    }

    fd = open_fixture();
    if (fd < 0)
    {
        write(1, "dyndup:open-dup3-fail\n", 22);
        return 10;
    }
    duplicated = dup3(fd, 8, O_CLOEXEC);
    if (duplicated != 8)
    {
        write(1, "dyndup:dup3-fail\n", 17);
        return 11;
    }
    flags = fcntl(duplicated, F_GETFD, 0);
    if ((flags < 0) || ((flags & FD_CLOEXEC) == 0))
    {
        write(1, "dyndup:dup3-cloexec-fail\n", 25);
        return 12;
    }
    if (read_nested_file_from_fd(duplicated) == 0)
    {
        write(1, "dyndup:dup3-read-fail\n", 22);
        return 13;
    }
    if ((close(duplicated) != 0) || (close(fd) != 0))
    {
        write(1, "dyndup:dup3-close-fail\n", 23);
        return 14;
    }

    write(stdout_dup, "dyndup:dup:dup2:dup3\n", 21);
    if (close(stdout_dup) != 0)
    {
        write(1, "dyndup:stdout-close-fail\n", 25);
        return 15;
    }

    return 0;
}
