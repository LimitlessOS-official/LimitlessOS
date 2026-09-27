#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t g_sigpipe_seen = 0;
static volatile sig_atomic_t g_sigchld_seen = 0;

static void write_literal(const char *text, unsigned long length)
{
    while (length != 0ul)
    {
        ssize_t written = write(1, text, length);
        if (written <= 0)
        {
            return;
        }
        text += written;
        length -= (unsigned long)written;
    }
}

static void sigpipe_handler(int signal_number)
{
    (void)signal_number;
    g_sigpipe_seen = 1;
    write_literal("sigpipe-caught\n", 15ul);
}

static void sigchld_handler(int signal_number)
{
    (void)signal_number;
    g_sigchld_seen = 1;
    write_literal("sigchld-caught\n", 15ul);
}

int main(void)
{
    struct sigaction action;
    int pipefd[2];
    pid_t child;
    int status;
    ssize_t written;

    action.sa_handler = sigpipe_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGPIPE, &action, 0) != 0)
    {
        return 10;
    }

    action.sa_handler = sigchld_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGCHLD, &action, 0) != 0)
    {
        return 11;
    }

    if (pipe(pipefd) != 0)
    {
        return 12;
    }
    if (close(pipefd[0]) != 0)
    {
        return 13;
    }

    errno = 0;
    written = write(pipefd[1], "x", 1);
    if ((written >= 0) || ((errno != EPIPE) && (g_sigpipe_seen == 0)))
    {
        return 14;
    }
    (void)close(pipefd[1]);
    if (g_sigpipe_seen == 0)
    {
        return 15;
    }

    child = fork();
    if (child < 0)
    {
        return 16;
    }
    if (child == 0)
    {
        _exit(0);
    }

    if (waitpid(child, &status, 0) != child)
    {
        return 17;
    }
    if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0))
    {
        return 18;
    }
    if (g_sigchld_seen == 0)
    {
        return 19;
    }

    write_literal("sigsmoke-done\n", 14ul);
    return 0;
}
