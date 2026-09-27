#include <unistd.h>

int main(void)
{
    static const char message[] = "dyngetpid-pass\n";
    pid_t pid = getpid();

    (void)write(1, message, sizeof(message) - 1u);
    return (pid > 0) ? 0 : 1;
}
