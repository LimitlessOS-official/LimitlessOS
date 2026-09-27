#include <string.h>
#include <stdio.h>

int main(void)
{
    char buffer[32];
    const char *source = "dynhelper-pass";
    size_t length = strlen(source);

    if (length != 14u) {
        return 2;
    }

    (void)memcpy(buffer, source, length + 1u);
    if (strcmp(buffer, source) != 0) {
        return 3;
    }

    return (puts(buffer) >= 0) ? 0 : 4;
}
