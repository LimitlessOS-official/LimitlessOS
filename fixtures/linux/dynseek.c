#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static int has_prefix(const char *value, const char *prefix)
{
    unsigned long index = 0;

    while (prefix[index] != 0)
    {
        if (value[index] != prefix[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

int main(void)
{
    struct stat path_stat;
    struct stat fd_stat;
    char buffer[32];
    int fd;
    ssize_t got;
    off_t pos;

    if (stat("/nvme/apps/data/file.txt", &path_stat) != 0)
    {
        return 1;
    }
    if (path_stat.st_size != 27)
    {
        return 2;
    }

    fd = open("/nvme/apps/data/file.txt", O_RDONLY);
    if (fd < 0)
    {
        return 3;
    }

    if (fstat(fd, &fd_stat) != 0)
    {
        close(fd);
        return 4;
    }
    if (fd_stat.st_size != path_stat.st_size)
    {
        close(fd);
        return 5;
    }

    pos = lseek(fd, 7, SEEK_SET);
    if (pos != 7)
    {
        close(fd);
        return 6;
    }

    got = read(fd, buffer, 8);
    if (got != 8)
    {
        close(fd);
        return 7;
    }
    buffer[8] = 0;
    if (has_prefix(buffer, "FAT32 pa") == 0)
    {
        close(fd);
        return 8;
    }

    pos = lseek(fd, -9, SEEK_END);
    if (pos != 18)
    {
        close(fd);
        return 9;
    }

    got = read(fd, buffer, 7);
    if (got != 7)
    {
        close(fd);
        return 10;
    }
    buffer[7] = 0;
    if (has_prefix(buffer, "fixture") == 0)
    {
        close(fd);
        return 11;
    }

    if (close(fd) != 0)
    {
        return 12;
    }

    if (write(1, "dynseek:FAT32 pa:fixture", 24) != 24)
    {
        return 13;
    }

    return 0;
}
