#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv, char **envp)
{
    char *p;
    char *q;
    char *z;
    const char *user;
    int i;

    if (argc != 1) return 2;
    if ((argv == 0) || (argv[0] == 0)) return 3;
    if ((envp == 0) || (envp[0] == 0)) return 4;

    p = (char *)malloc(32);
    if (p == 0) return 5;
    strcpy(p, "heap-live");
    if (strcmp(p, "heap-live") != 0) return 6;

    q = (char *)realloc(p, 64);
    if (q == 0) return 7;
    if (strcmp(q, "heap-live") != 0) return 8;
    strcpy(q, "realloc-live");
    if (strcmp(q, "realloc-live") != 0) return 9;
    free(q);

    z = (char *)calloc(4, 8);
    if (z == 0) return 10;
    for (i = 0; i < 32; ++i)
    {
        if (z[i] != 0) return 11;
    }
    strcpy(z, "calloc-live");
    if (strcmp(z, "calloc-live") != 0) return 12;
    free(z);

    user = getenv("USER");
    if ((user == 0) || (strcmp(user, "limitless") != 0)) return 13;
    if (setenv("USER", "pilot", 1) != 0) return 14;
    user = getenv("USER");
    if ((user == 0) || (strcmp(user, "pilot") != 0)) return 15;

    if (puts("dynheap-pass") < 0) return 16;
    if (puts("dynsetenv-pass") < 0) return 17;

    return 0;
}
