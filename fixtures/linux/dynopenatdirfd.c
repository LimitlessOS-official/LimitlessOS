#define O_RDONLY 0

extern int openat(int dirfd, const char *path, int flags, ...);
extern long read(int fd, void *buffer, unsigned long count);
extern int close(int fd);
extern long write(int fd, const void *buffer, unsigned long count);

static unsigned long cstr_len(const char *text)
{
    unsigned long length;

    length = 0ul;
    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static int write_text(const char *text)
{
    unsigned long length;

    length = cstr_len(text);
    return (write(1, text, length) == (long)length) ? 0 : -1;
}

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

int main(void)
{
    char buffer[28];
    char output[64];
    int dirfd;
    int fd;
    long got;
    unsigned long index;
    unsigned long offset;

    dirfd = openat(-100, "/nvme/apps/data", O_RDONLY, 0);
    if (dirfd < 0)
    {
        (void)write_text("dynopenatdirfd:dir-open-fail\n");
        return 1;
    }

    fd = openat(dirfd, "file.txt", O_RDONLY, 0);
    if (fd < 0)
    {
        (void)close(dirfd);
        (void)write_text("dynopenatdirfd:file-open-fail\n");
        return 2;
    }

    got = read(fd, buffer, 27ul);
    (void)close(fd);
    (void)close(dirfd);
    if (got != 27)
    {
        (void)write_text("dynopenatdirfd:read-fail\n");
        return 3;
    }

    buffer[27] = '\0';
    if (has_prefix(buffer, "Nested FAT32 path fixture") == 0)
    {
        (void)write_text("dynopenatdirfd:content-fail\n");
        return 4;
    }

    offset = 0ul;
    output[offset++] = 'd';
    output[offset++] = 'y';
    output[offset++] = 'n';
    output[offset++] = 'o';
    output[offset++] = 'p';
    output[offset++] = 'e';
    output[offset++] = 'n';
    output[offset++] = 'a';
    output[offset++] = 't';
    output[offset++] = 'd';
    output[offset++] = 'i';
    output[offset++] = 'r';
    output[offset++] = 'f';
    output[offset++] = 'd';
    output[offset++] = ':';
    for (index = 0ul; index < 6ul; ++index)
    {
        output[offset++] = buffer[index];
    }
    output[offset++] = ':';
    for (index = 7ul; index < 12ul; ++index)
    {
        output[offset++] = buffer[index];
    }
    output[offset++] = '\n';

    if (write(1, output, offset) != (long)offset)
    {
        return 5;
    }

    return 0;
}
