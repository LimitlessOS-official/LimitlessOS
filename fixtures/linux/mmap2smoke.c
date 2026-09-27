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

static uint32_t checksum_bytes(const unsigned char *bytes, unsigned long count)
{
    uint32_t digest = 2166136261u;
    unsigned long index;

    for (index = 0u; index < count; ++index)
    {
        digest ^= (uint32_t)bytes[index];
        digest *= 16777619u;
    }
    return digest;
}

static int expect_pattern(const unsigned char *bytes, unsigned long start, unsigned long count)
{
    unsigned long index;
    unsigned char expected;

    for (index = 0u; index < count; ++index)
    {
        expected = (unsigned char)(((start + index) * 37u + 11u) & 0xFFu);
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
    void *full_map;
    void *page_map;
    uint32_t full_checksum;
    uint32_t page_checksum;

    write_text("mmap2smoke-start\n");
    fd = open("/nvme/apps/mmapdata", O_RDONLY);
    if (fd < 0)
    {
        write_text("mmap2smoke-open-failed\n");
        return 1;
    }

    full_map = mmap(0, 8192, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
    if (full_map == MAP_FAILED)
    {
        if (errno == EINVAL)
        {
            write_text("mmap2smoke-mmap-denied-einval\n");
        }
        else
        {
            write_text("mmap2smoke-mmap-denied-other\n");
        }
        (void)close(fd);
        return 0;
    }
    if (expect_pattern((const unsigned char *)full_map, 0u, 8192u) == 0)
    {
        write_text("mmap2smoke-full-mismatch\n");
        (void)munmap(full_map, 8192);
        (void)close(fd);
        return 2;
    }
    full_checksum = checksum_bytes((const unsigned char *)full_map, 8192u);
    (void)full_checksum;
    write_text("mmap2smoke-full-ok\n");

    page_map = mmap(0, 4096, PROT_READ, MAP_PRIVATE, fd, 4096);
    if (page_map == MAP_FAILED)
    {
        write_text("mmap2smoke-offset-denied\n");
        (void)munmap(full_map, 8192);
        (void)close(fd);
        return 3;
    }
    if (expect_pattern((const unsigned char *)page_map, 4096u, 4096u) == 0)
    {
        write_text("mmap2smoke-offset-mismatch\n");
        (void)munmap(page_map, 4096);
        (void)munmap(full_map, 8192);
        (void)close(fd);
        return 4;
    }
    page_checksum = checksum_bytes((const unsigned char *)page_map, 4096u);
    if ((full_checksum == 0u) || (page_checksum == 0u))
    {
        write_text("mmap2smoke-zero-checksum\n");
        (void)munmap(page_map, 4096);
        (void)munmap(full_map, 8192);
        (void)close(fd);
        return 5;
    }

    write_text("mmap2smoke-offset-ok\n");
    (void)munmap(page_map, 4096);
    (void)munmap(full_map, 8192);
    (void)close(fd);
    write_text("mmap2smoke-done\n");
    return 0;
}
