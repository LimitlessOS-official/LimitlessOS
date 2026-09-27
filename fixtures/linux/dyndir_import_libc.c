typedef unsigned long size_t;
typedef long ssize_t;
typedef long off_t;

int __libc_start_main(
    int (*main_fn)(int, char **, char **),
    int argc,
    char **argv,
    void (*init_fn)(void),
    void (*fini_fn)(void),
    void (*rtld_fini_fn)(void),
    void *stack_end)
{
    (void)main_fn;
    (void)argc;
    (void)argv;
    (void)init_fn;
    (void)fini_fn;
    (void)rtld_fini_fn;
    (void)stack_end;
    return -38;
}

ssize_t read(int fd, void *buffer, size_t count)
{
    (void)fd;
    (void)buffer;
    (void)count;
    return -38;
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    (void)fd;
    (void)buffer;
    (void)count;
    return -38;
}

int open(const char *path, int flags, ...)
{
    (void)path;
    (void)flags;
    return -38;
}

int openat(int dirfd, const char *path, int flags, ...)
{
    (void)dirfd;
    (void)path;
    (void)flags;
    return -38;
}

int close(int fd)
{
    (void)fd;
    return -38;
}

off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return -38;
}

int stat(const char *path, void *statbuf)
{
    (void)path;
    (void)statbuf;
    return -38;
}

int fstat(int fd, void *statbuf)
{
    (void)fd;
    (void)statbuf;
    return -38;
}

long getdents64(unsigned int fd, void *dirp, unsigned int count)
{
    (void)fd;
    (void)dirp;
    (void)count;
    return -38;
}

long getcwd(char *buffer, unsigned long size)
{
    (void)buffer;
    (void)size;
    return -38;
}

long chdir(const char *path)
{
    (void)path;
    return -38;
}

long fchdir(int fd)
{
    (void)fd;
    return -38;
}

long fcntl(int fd, int command, ...)
{
    (void)fd;
    (void)command;
    return -38;
}

int dup(int oldfd)
{
    (void)oldfd;
    return -38;
}

int dup2(int oldfd, int newfd)
{
    (void)oldfd;
    (void)newfd;
    return -38;
}

int dup3(int oldfd, int newfd, int flags)
{
    (void)oldfd;
    (void)newfd;
    (void)flags;
    return -38;
}

int pipe(int pipefd[2])
{
    (void)pipefd;
    return -38;
}

int fork(void)
{
    return -38;
}

long wait4(long pid, int *status, int options, void *rusage)
{
    (void)pid;
    (void)status;
    (void)options;
    (void)rusage;
    return -38;
}

long newfstatat(int dirfd, const char *path, void *statbuf, int flags)
{
    (void)dirfd;
    (void)path;
    (void)statbuf;
    (void)flags;
    return -38;
}

long readlink(const char *path, char *buffer, unsigned long size)
{
    (void)path;
    (void)buffer;
    (void)size;
    return -38;
}

long readv(int fd, const void *iov, int iovcnt)
{
    (void)fd;
    (void)iov;
    (void)iovcnt;
    return -38;
}

long writev(int fd, const void *iov, int iovcnt)
{
    (void)fd;
    (void)iov;
    (void)iovcnt;
    return -38;
}

long poll(void *fds, unsigned long nfds, int timeout)
{
    (void)fds;
    (void)nfds;
    (void)timeout;
    return -38;
}
