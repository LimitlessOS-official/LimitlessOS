#define O_RDONLY 0

extern long linux_getcwd(char *buffer, unsigned long size) __asm__("getcwd");
extern long linux_chdir(const char *path) __asm__("chdir");
extern long linux_readlink(const char *path, char *buffer, unsigned long size) __asm__("readlink");
extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern long read(int fd, void *buffer, unsigned long count);
extern long write(int fd, const void *buffer, unsigned long count);

static long write_all(const char *text, unsigned long length)
{
    unsigned long written;
    long result;

    written = 0ul;
    while (written < length)
    {
        result = write(1, text + written, length - written);
        if (result <= 0)
        {
            return result;
        }
        written += (unsigned long)result;
    }

    return (long)written;
}

static unsigned long string_length(const char *value)
{
    unsigned long length;

    length = 0ul;
    while (value[length] != '\0')
    {
        ++length;
    }
    return length;
}

static int string_equals(const char *left, const char *right)
{
    unsigned long index;

    index = 0ul;
    while ((left[index] != '\0') || (right[index] != '\0'))
    {
        if (left[index] != right[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
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
    char cwd[64];
    char file_buffer[32];
    char link_buffer[64];
    long result;
    int fd;

    result = linux_getcwd(cwd, sizeof(cwd));
    if ((result <= 0) || (string_equals(cwd, "/") == 0))
    {
        (void)write_all("dyncwd:getcwd-root-fail\n", 24ul);
        return 1;
    }

    if (linux_chdir("/nvme/apps") != 0)
    {
        (void)write_all("dyncwd:chdir-fail\n", 18ul);
        return 2;
    }

    result = linux_getcwd(cwd, sizeof(cwd));
    if ((result <= 0) || (string_equals(cwd, "/nvme/apps") == 0))
    {
        (void)write_all("dyncwd:getcwd-apps-fail\n", 24ul);
        return 3;
    }

    fd = open("data/file.txt", O_RDONLY);
    if (fd < 0)
    {
        (void)write_all("dyncwd:open-relative-fail\n", 26ul);
        return 4;
    }

    result = read(fd, file_buffer, 27ul);
    if (result != 27)
    {
        (void)close(fd);
        (void)write_all("dyncwd:read-relative-fail\n", 26ul);
        return 5;
    }
    file_buffer[27] = '\0';
    (void)close(fd);

    if (has_prefix(file_buffer, "Nested FAT32 path fixture") == 0)
    {
        (void)write_all("dyncwd:file-content-fail\n", 25ul);
        return 6;
    }

    result = linux_readlink("/proc/self/exe", link_buffer, sizeof(link_buffer) - 1ul);
    if (result <= 0)
    {
        (void)write_all("dyncwd:readlink-fail\n", 21ul);
        return 7;
    }
    link_buffer[result] = '\0';
    if (string_equals(link_buffer, "/proc/self/exe") == 0)
    {
        (void)write_all("dyncwd:readlink-target-fail\n", 28ul);
        return 8;
    }

    (void)write_all("dyncwd:", 7ul);
    (void)write_all(cwd, string_length(cwd));
    (void)write_all(":", 1ul);
    (void)write_all(file_buffer, 25ul);
    (void)write_all(":", 1ul);
    (void)write_all(link_buffer, string_length(link_buffer));
    (void)write_all("\n", 1ul);

    return 0;
}
