#include <unistd.h>

int main(void)
{
    static const char message[] = "dynsmoke-start\n";

    (void)write(1, message, sizeof(message) - 1u);
    return 0;
}
