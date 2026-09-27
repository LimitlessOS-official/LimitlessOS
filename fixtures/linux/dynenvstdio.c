#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv, char **envp)
{
    const char *path = getenv("PATH");
    const char *user = getenv("USER");
    const char *home = getenv("HOME");
    const char *pwd = getenv("PWD");

    if (argc != 1) {
        return 2;
    }
    if ((argv == 0) || (argv[0] == 0) || (argv[0][0] == 0)) {
        return 3;
    }
    if ((envp == 0) || (envp[0] == 0)) {
        return 4;
    }
    if ((path == 0) || (strcmp(path, "/usr/local/bin:/bin:/usr/bin") != 0)) {
        return 5;
    }
    if ((user == 0) || (strcmp(user, "limitless") != 0)) {
        return 6;
    }
    if ((home == 0) || (strcmp(home, "/") != 0)) {
        return 7;
    }
    if ((pwd == 0) || (strcmp(pwd, "/") != 0)) {
        return 8;
    }

    if (printf("dynprintf-pass\n") < 0) {
        return 9;
    }
    if (fputs("dynfputs-pass\n", (FILE *)1) < 0) {
        return 10;
    }
    if (fwrite("dynfwrite-pass\n", 1, 15, (FILE *)1) != 15) {
        return 11;
    }

    return 0;
}
