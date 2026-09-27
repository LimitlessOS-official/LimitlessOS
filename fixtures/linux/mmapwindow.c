#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

static void write_text(const char *text)
{
    const char *cursor = text;
    while (*cursor != '\0')
    {
        ++cursor;
    }
    (void)write(1, text, (size_t)(cursor - text));
}

static int expect_pattern(const unsigned char *bytes, unsigned long start, unsigned long count)
{
    unsigned long index;
    unsigned char expected;

    for (index = 0u; index < count; ++index)
    {
        expected = (unsigned char)(((start + index) * 29u + 7u) & 0xFFu);
        if (bytes[index] != expected)
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    int fd;
    void *window;

    write_text("mmapwindow-start\n");
    fd = open("/nvme/apps/bigdata", O_RDONLY);
    if (fd < 0)
    {
        write_text("mmapwindow-open-failed\n");
        return 1;
    }

    window = mmap(0, 4096, PROT_READ, MAP_PRIVATE, fd, 65536);
    if (window == MAP_FAILED)
    {
        if (errno == EINVAL)
        {
            write_text("mmapwindow-mmap-denied-einval\n");
        }
        else
        {
            write_text("mmapwindow-mmap-denied-other\n");
        }
        (void)close(fd);
        return 0;
    }

    if (expect_pattern((const unsigned char *)window, 65536u, 4096u) == 0)
    {
        write_text("mmapwindow-mismatch\n");
        (void)munmap(window, 4096);
        (void)close(fd);
        return 2;
    }

    write_text("mmapwindow-window-ok\n");
    (void)munmap(window, 4096);
    (void)close(fd);
    write_text("mmapwindow-done\n");
    return 0;
}
