#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    char buffer[128];
    int fd;
    ssize_t got;

    fd = open("/nvme/apps/data/file.txt", O_RDONLY);
    if (fd < 0)
    {
        return 1;
    }

    got = read(fd, buffer, sizeof(buffer));
    if (got <= 0)
    {
        close(fd);
        return 2;
    }

    if (close(fd) != 0)
    {
        return 3;
    }

    if (write(1, "dynfileio:", 10) != 10)
    {
        return 4;
    }

    if (write(1, buffer, (unsigned long)got) != got)
    {
        return 5;
    }

    return 0;
}
