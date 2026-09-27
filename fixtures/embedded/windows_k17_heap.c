/*
 * K17 verifier PE source. Regenerate with:
 * x86_64-w64-mingw32-gcc -Os -s -nostdlib -Wl,-e,mainCRTStartup \
 *     -Wl,--subsystem,console -o heap.exe windows_k17_heap.c -lkernel32 -lntdll
 */
#include <windows.h>

__declspec(dllimport) LONG NtCreateEvent(
    HANDLE *event_handle,
    DWORD desired_access,
    void *object_attributes,
    DWORD event_type,
    BOOLEAN initial_state);

void mainCRTStartup(void)
{
    static const char text[] = "K17 heap\n";
    DWORD written = 0;
    LARGE_INTEGER zero;
    LARGE_INTEGER position;
    HANDLE event_handle = 0;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE heap = GetProcessHeap();
    char *buffer = (char *)HeapAlloc(heap, 0, sizeof(text) - 1u);
    int ok = 1;
    unsigned int index;

    zero.QuadPart = 0;
    position.QuadPart = -1;

    if ((out == 0) || (heap == 0) || (buffer == 0))
    {
        ok = 0;
    }
    if ((NtCreateEvent(&event_handle, 3u, 0, 0u, 0) != 0) || (event_handle == 0))
    {
        ok = 0;
    }
    if ((SetFilePointerEx(out, zero, &position, FILE_BEGIN) == 0)
        || (position.QuadPart != 0))
    {
        ok = 0;
    }

    if (buffer != 0)
    {
        for (index = 0u; index < (sizeof(text) - 1u); ++index)
        {
            buffer[index] = text[index];
        }
        if ((WriteConsoleA(out, buffer, (DWORD)(sizeof(text) - 1u), &written, 0) == 0)
            || (written != (DWORD)(sizeof(text) - 1u)))
        {
            ok = 0;
        }
        if (HeapFree(heap, 0, buffer) == 0)
        {
            ok = 0;
        }
    }

    if ((event_handle != 0) && (CloseHandle(event_handle) == 0))
    {
        ok = 0;
    }

    ExitProcess(ok != 0 ? 0 : 0xE1u);
}
