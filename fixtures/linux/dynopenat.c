#define AT_FDCWD (-100)
#define O_RDONLY 0

extern long chdir(const char *path);
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
    char output[48];
    int fd;
    long got;
    unsigned long index;
    unsigned long offset;

    if (chdir("/nvme/apps") != 0)
    {
        (void)write_text("dynopenat:chdir-fail\n");
        return 1;
    }

    fd = openat(AT_FDCWD, "data/file.txt", O_RDONLY, 0);
    if (fd < 0)
    {
        (void)write_text("dynopenat:openat-fail\n");
        return 2;
    }

    got = read(fd, buffer, 27ul);
    (void)close(fd);
    if (got != 27)
    {
        (void)write_text("dynopenat:read-fail\n");
        return 3;
    }

    buffer[27] = '\0';
    if (has_prefix(buffer, "Nested FAT32 path fixture") == 0)
    {
        (void)write_text("dynopenat:content-fail\n");
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
