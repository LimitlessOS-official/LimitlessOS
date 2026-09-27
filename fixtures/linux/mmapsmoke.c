#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static void write_literal(const char *text, unsigned long bytes)
{
    (void)write(1, text, bytes);
}

#define WRITE_LITERAL(text) write_literal((text), sizeof(text) - 1)

static void write_u32(unsigned int value)
{
    char digits[10];
    unsigned int index = 0;

    if (value == 0) {
        const char zero[] = "0";
        (void)write(1, zero, sizeof(zero) - 1);
        return;
    }

    while ((value != 0) && (index < sizeof(digits))) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index != 0) {
        --index;
        (void)write(1, &digits[index], 1);
    }
}

static unsigned int checksum_bytes(const unsigned char *bytes, unsigned long length)
{
    unsigned int hash = 2166136261u;
    unsigned long index;

    for (index = 0; index < length; ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }

    return hash;
}

int main(void)
{
    const char path[] = "/nvme/apps/data/file.txt";
    unsigned char read_buffer[64];
    void *mapped;
    int fd;
    ssize_t read_bytes;
    unsigned int checksum;

    WRITE_LITERAL("mmapsmoke-start\n");

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        WRITE_LITERAL("mmapsmoke-open-failed errno ");
        write_u32((unsigned int)errno);
        WRITE_LITERAL("\n");
        return 2;
    }

    read_bytes = read(fd, read_buffer, sizeof(read_buffer));
    if (read_bytes < 0) {
        WRITE_LITERAL("mmapsmoke-read-failed errno ");
        write_u32((unsigned int)errno);
        WRITE_LITERAL("\n");
        (void)close(fd);
        return 3;
    }

    checksum = checksum_bytes(read_buffer, (unsigned long)read_bytes);
    WRITE_LITERAL("mmapsmoke-read-bytes ");
    write_u32((unsigned int)read_bytes);
    WRITE_LITERAL(" checksum ");
    write_u32(checksum);
    WRITE_LITERAL("\n");

    mapped = mmap(0, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        WRITE_LITERAL("mmapsmoke-mmap-failed errno ");
        write_u32((unsigned int)errno);
        WRITE_LITERAL("\n");
        (void)close(fd);
        return 0;
    }

    checksum = checksum_bytes((const unsigned char *)mapped, (unsigned long)read_bytes);
    WRITE_LITERAL("mmapsmoke-mmap-ok checksum ");
    write_u32(checksum);
    WRITE_LITERAL("\n");

    (void)munmap(mapped, 4096);
    (void)close(fd);
    WRITE_LITERAL("mmapsmoke-done\n");
    return 0;
}
