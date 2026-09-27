/*
 * K16 verifier PE source. Regenerate with:
 * x86_64-w64-mingw32-gcc -Os -s -nostdlib -Wl,-e,mainCRTStartup \
 *     -Wl,--subsystem,console -o hello.exe windows_k16_hello.c -lkernel32
 */
#include <windows.h>

void mainCRTStartup(void)
{
    DWORD written = 0;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    WriteConsoleA(out, "K16\n", 4, &written, 0);
    ExitProcess((written == 4) ? 0 : 0xEE);
}
