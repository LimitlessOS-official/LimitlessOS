#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern long getdents64(unsigned int fd, void *dirp, unsigned int count);

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

static int name_is_data(const char *name)
{
    if ((name[0] == 'd') || (name[0] == 'D'))
    {
        if ((name[1] == 'a') || (name[1] == 'A'))
        {
            if ((name[2] == 't') || (name[2] == 'T'))
            {
                if ((name[3] == 'a') || (name[3] == 'A'))
                {
                    return name[4] == '\0';
                }
            }
        }
    }

    return 0;
}

int main(void)
{
    char buffer[512];
    struct stat st;
    long bytes;
    long offset;
    int fd;
    int saw_data;
    int stat_data;

    saw_data = 0;
    stat_data = 0;
    fd = open("/nvme/apps", O_RDONLY);
    if (fd < 0)
    {
        (void)write_all("dyndir:open-fail\n", 17ul);
        return 1;
    }

    bytes = getdents64((unsigned int)fd, buffer, (unsigned int)sizeof(buffer));
    if (bytes <= 0)
    {
        (void)close(fd);
        (void)write_all("dyndir:getdents-fail\n", 21ul);
        return 2;
    }

    offset = 0;
    while ((offset + 19) <= bytes)
    {
        unsigned short record_bytes;
        const char *name;

        record_bytes = *(const unsigned short *)(const void *)(buffer + offset + 16);
        if ((record_bytes < 20u) || ((offset + (long)record_bytes) > bytes))
        {
            (void)close(fd);
            (void)write_all("dyndir:record-fail\n", 19ul);
            return 3;
        }

        name = (const char *)(const void *)(buffer + offset + 19);
        if (name_is_data(name) != 0)
        {
            saw_data = 1;
        }

        offset += (long)record_bytes;
    }

    (void)close(fd);
    if (saw_data == 0)
    {
        (void)write_all("dyndir:data-missing\n", 20ul);
        return 4;
    }

    if (stat("/nvme/apps/data", &st) == 0)
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)
        {
            stat_data = 1;
        }
    }

    if (stat_data == 0)
    {
        (void)write_all("dyndir:stat-fail\n", 17ul);
        return 5;
    }

    (void)write_all("dyndir:data\n", 12ul);
    return 0;
}
