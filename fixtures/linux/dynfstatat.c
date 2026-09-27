#define AT_FDCWD (-100)
#define S_IFMT 0170000u
#define S_IFREG 0100000u
#define S_IFDIR 0040000u

typedef unsigned int u32;
typedef unsigned long u64;

typedef struct linux_stat64
{
    u64 st_dev;
    u64 st_ino;
    u64 st_nlink;
    u32 st_mode;
    u32 st_uid;
    u32 st_gid;
    u32 __pad0;
    u64 st_rdev;
    u64 st_size;
    u64 st_blksize;
    u64 st_blocks;
    u64 st_atime;
    u64 st_atime_nsec;
    u64 st_mtime;
    u64 st_mtime_nsec;
    u64 st_ctime;
    u64 st_ctime_nsec;
    u64 __unused[3];
} linux_stat64_t;

extern long chdir(const char *path);
extern long newfstatat(int dirfd, const char *path, linux_stat64_t *statbuf, int flags);
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

static unsigned long append_u64(char *target, unsigned long offset, u64 value)
{
    char scratch[21];
    unsigned long count;

    count = 0ul;
    if (value == 0ull)
    {
        target[offset] = '0';
        return offset + 1ul;
    }

    while (value != 0ull)
    {
        scratch[count] = (char)('0' + (char)(value % 10ull));
        value /= 10ull;
        ++count;
    }

    while (count != 0ul)
    {
        --count;
        target[offset] = scratch[count];
        ++offset;
    }

    return offset;
}

int main(void)
{
    linux_stat64_t file_stat;
    linux_stat64_t dir_stat;
    char output[64];
    unsigned long offset;

    if (chdir("/nvme/apps") != 0)
    {
        (void)write_text("dynfstatat:chdir-fail\n");
        return 1;
    }

    if (newfstatat(AT_FDCWD, "data/file.txt", &file_stat, 0) != 0)
    {
        (void)write_text("dynfstatat:file-fail\n");
        return 2;
    }

    if (newfstatat(AT_FDCWD, "data", &dir_stat, 0) != 0)
    {
        (void)write_text("dynfstatat:dir-fail\n");
        return 3;
    }

    if ((file_stat.st_size != 27ull)
        || ((file_stat.st_mode & S_IFMT) != S_IFREG)
        || ((dir_stat.st_mode & S_IFMT) != S_IFDIR))
    {
        (void)write_text("dynfstatat:metadata-fail\n");
        return 4;
    }

    offset = 0ul;
    output[offset++] = 'd';
    output[offset++] = 'y';
    output[offset++] = 'n';
    output[offset++] = 'f';
    output[offset++] = 's';
    output[offset++] = 't';
    output[offset++] = 'a';
    output[offset++] = 't';
    output[offset++] = 'a';
    output[offset++] = 't';
    output[offset++] = ':';
    offset = append_u64(output, offset, file_stat.st_size);
    output[offset++] = ':';
    output[offset++] = 'f';
    output[offset++] = 'i';
    output[offset++] = 'l';
    output[offset++] = 'e';
    output[offset++] = '-';
    output[offset++] = 'd';
    output[offset++] = 'i';
    output[offset++] = 'r';
    output[offset++] = '\n';

    if (write(1, output, offset) != (long)offset)
    {
        return 5;
    }

    return 0;
}
