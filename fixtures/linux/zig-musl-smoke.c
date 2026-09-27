#include <unistd.h>

int main(void)
{
    write(1, "zig-musl-smoke\n", 15);
    return 0;
}
