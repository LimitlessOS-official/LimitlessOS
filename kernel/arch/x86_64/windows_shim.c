#include "windows_shim_x64.h"

#include "paging_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "syscall_x64.h"
#include "vma_x64.h"
#include "windows_abi_x64.h"

/*
 * K.13 adds the first LimitlessOS-owned ntdll shim DLL surface, K.14 adds
 * the kernel32 shim registry surface that Win32 console applications import,
 * and K.15 adds the msvcrt/ucrtbase C runtime shim registry layer.
 * This integrates with pe64_x64.h for PE32+ metadata validation, vma_x64.h for
 * brokered user mapping, persona_x64.h for Windows process state, and
 * windows_abi_x64.h for truthful NTSTATUS values. The checkpoint proves compact
 * embedded PE64 DLLs are parsed, mapped as user RX/RO pages, and expose the
 * documented ntdll/kernel32/CRT switchboard addresses. ntdll's
 * LdrInitializeThunk now performs the minimum native EXE handoff by chaining to
 * the PE entry pointer supplied by pe64.c. The kernel32 console path exposes
 * live GetStdHandle/WriteConsoleA/WriteFile/pseudo-handle helpers backed by the
 * Windows NT ABI switchboard; kernel32 and CRT exports that cannot execute
 * natively yet remain explicit unavailable stubs, so the scaffold verifies
 * linkage/import resolution without fabricated Win32 success.
 */

typedef struct windows_shim64_export
{
    const char *name;
    u16 ordinal;
    u32 rva;
} windows_shim64_export_t;

static const windows_shim64_export_t g_windows_shim64_ntdll_exports[
    WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT] = {
    { "LdrInitializeThunk", 1u, WINDOWS_SHIM64_NTDLL_RVA_LDR_INITIALIZE_THUNK },
    { "LdrLoadDll", 2u, WINDOWS_SHIM64_NTDLL_RVA_LDR_LOAD_DLL },
    { "RtlAllocateHeap", 3u, WINDOWS_SHIM64_NTDLL_RVA_RTL_ALLOCATE_HEAP },
    { "RtlFreeHeap", 4u, WINDOWS_SHIM64_NTDLL_RVA_RTL_FREE_HEAP },
    { "RtlReAllocateHeap", 5u, WINDOWS_SHIM64_NTDLL_RVA_RTL_REALLOCATE_HEAP },
    { "RtlCreateHeap", 6u, WINDOWS_SHIM64_NTDLL_RVA_RTL_CREATE_HEAP },
    { "RtlUserThreadStart", 7u, WINDOWS_SHIM64_NTDLL_RVA_RTL_USER_THREAD_START },
    { "KiUserExceptionDispatcher", 8u, WINDOWS_SHIM64_NTDLL_RVA_KI_USER_EXCEPTION_DISPATCHER },
    { "NtdllDefWindowProc_W", 9u, WINDOWS_SHIM64_NTDLL_RVA_NTDLL_DEF_WINDOW_PROC_W },
    { "NtWaitForSingleObject", 10u, WINDOWS_SHIM64_NTDLL_RVA_NT_WAIT_FOR_SINGLE_OBJECT },
    { "NtReadFile", 11u, WINDOWS_SHIM64_NTDLL_RVA_NT_READ_FILE },
    { "NtWriteFile", 12u, WINDOWS_SHIM64_NTDLL_RVA_NT_WRITE_FILE },
    { "NtOpenKey", 13u, WINDOWS_SHIM64_NTDLL_RVA_NT_OPEN_KEY },
    { "NtQueryValueKey", 14u, WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_VALUE_KEY },
    { "NtAllocateVirtualMemory", 15u, WINDOWS_SHIM64_NTDLL_RVA_NT_ALLOCATE_VIRTUAL_MEMORY },
    { "NtQueryInformationProcess", 16u, WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_INFORMATION_PROCESS },
    { "NtCreateKey", 17u, WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_KEY },
    { "NtFreeVirtualMemory", 18u, WINDOWS_SHIM64_NTDLL_RVA_NT_FREE_VIRTUAL_MEMORY },
    { "NtQuerySystemInformation", 19u, WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_SYSTEM_INFORMATION },
    { "NtProtectVirtualMemory", 20u, WINDOWS_SHIM64_NTDLL_RVA_NT_PROTECT_VIRTUAL_MEMORY },
    { "NtCreateFile", 21u, WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_FILE },
    { "NtCreateEvent", 22u, WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_EVENT },
    { "NtCreateMutant", 23u, WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_MUTANT },
    { "NtReleaseMutant", 24u, WINDOWS_SHIM64_NTDLL_RVA_NT_RELEASE_MUTANT },
    { "NtSetEvent", 25u, WINDOWS_SHIM64_NTDLL_RVA_NT_SET_EVENT }
};

static const windows_shim64_export_t g_windows_shim64_kernel32_exports[
    WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT] = {
    { "ExitProcess", 1u, WINDOWS_SHIM64_KERNEL32_RVA_EXIT_PROCESS },
    { "GetStdHandle", 2u, WINDOWS_SHIM64_KERNEL32_RVA_GET_STD_HANDLE },
    { "WriteConsoleA", 3u, WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_A },
    { "WriteConsoleW", 4u, WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_W },
    { "ReadFile", 5u, WINDOWS_SHIM64_KERNEL32_RVA_READ_FILE },
    { "WriteFile", 6u, WINDOWS_SHIM64_KERNEL32_RVA_WRITE_FILE },
    { "CreateFileA", 7u, WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_A },
    { "CreateFileW", 8u, WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_W },
    { "CloseHandle", 9u, WINDOWS_SHIM64_KERNEL32_RVA_CLOSE_HANDLE },
    { "VirtualAlloc", 10u, WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_ALLOC },
    { "VirtualFree", 11u, WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_FREE },
    { "VirtualProtect", 12u, WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_PROTECT },
    { "GetLastError", 13u, WINDOWS_SHIM64_KERNEL32_RVA_GET_LAST_ERROR },
    { "SetLastError", 14u, WINDOWS_SHIM64_KERNEL32_RVA_SET_LAST_ERROR },
    { "GetCurrentProcess", 15u, WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_PROCESS },
    { "GetCurrentThread", 16u, WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_THREAD },
    { "GetSystemInfo", 17u, WINDOWS_SHIM64_KERNEL32_RVA_GET_SYSTEM_INFO },
    { "GetTickCount64", 18u, WINDOWS_SHIM64_KERNEL32_RVA_GET_TICK_COUNT64 },
    { "QueryPerformanceCounter", 19u, WINDOWS_SHIM64_KERNEL32_RVA_QUERY_PERFORMANCE_COUNTER },
    { "GetProcessHeap", 20u, WINDOWS_SHIM64_KERNEL32_RVA_GET_PROCESS_HEAP },
    { "HeapAlloc", 21u, WINDOWS_SHIM64_KERNEL32_RVA_HEAP_ALLOC },
    { "HeapFree", 22u, WINDOWS_SHIM64_KERNEL32_RVA_HEAP_FREE },
    { "HeapReAlloc", 23u, WINDOWS_SHIM64_KERNEL32_RVA_HEAP_REALLOC },
    { "LoadLibraryA", 24u, WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_A },
    { "LoadLibraryW", 25u, WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_W },
    { "GetProcAddress", 26u, WINDOWS_SHIM64_KERNEL32_RVA_GET_PROC_ADDRESS }
};

static const u8 g_windows_shim64_kernel32_get_std_handle_helper[] = {
    0xB8u, 0x04u, 0x00u, 0x00u, 0x00u,
    0x81u, 0xF9u, 0xF5u, 0xFFu, 0xFFu, 0xFFu,
    0x74u, 0x1Cu,
    0xB8u, 0x08u, 0x00u, 0x00u, 0x00u,
    0x81u, 0xF9u, 0xF4u, 0xFFu, 0xFFu, 0xFFu,
    0x74u, 0x0Fu,
    0xB8u, 0x0Cu, 0x00u, 0x00u, 0x00u,
    0x81u, 0xF9u, 0xF6u, 0xFFu, 0xFFu, 0xFFu,
    0x74u, 0x02u,
    0x31u, 0xC0u,
    0xC3u
};

static const u8 g_windows_shim64_kernel32_write_file_helper[] = {
    0x48u, 0x83u, 0xECu, 0x78u,
    0x4Cu, 0x89u, 0x4Cu, 0x24u, 0x50u,
    0x4Cu, 0x8Du, 0x54u, 0x24u, 0x58u,
    0x4Cu, 0x89u, 0x54u, 0x24u, 0x28u,
    0x48u, 0x89u, 0x54u, 0x24u, 0x30u,
    0x4Cu, 0x89u, 0x44u, 0x24u, 0x38u,
    0x45u, 0x31u, 0xD2u,
    0x4Cu, 0x89u, 0x54u, 0x24u, 0x40u,
    0x4Cu, 0x89u, 0x54u, 0x24u, 0x48u,
    0x31u, 0xD2u,
    0x45u, 0x31u, 0xC0u,
    0x45u, 0x31u, 0xC9u,
    0x4Cu, 0x8Bu, 0xD1u,
    0xB8u, 0x08u, 0x00u, 0x00u, 0x00u,
    0x0Fu, 0x05u,
    0x85u, 0xC0u,
    0x75u, 0x1Cu,
    0x4Cu, 0x8Bu, 0x54u, 0x24u, 0x50u,
    0x4Du, 0x85u, 0xD2u,
    0x74u, 0x08u,
    0x4Cu, 0x8Bu, 0x5Cu, 0x24u, 0x60u,
    0x45u, 0x89u, 0x1Au,
    0xB8u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x48u, 0x83u, 0xC4u, 0x78u,
    0xC3u,
    0x31u, 0xC0u,
    0x48u, 0x83u, 0xC4u, 0x78u,
    0xC3u
};

static const windows_shim64_export_t g_windows_shim64_crt_exports[
    WINDOWS_SHIM64_CRT_SYMBOL_COUNT] = {
    { "printf", 1u, WINDOWS_SHIM64_CRT_RVA_PRINTF },
    { "malloc", 2u, WINDOWS_SHIM64_CRT_RVA_MALLOC },
    { "free", 3u, WINDOWS_SHIM64_CRT_RVA_FREE },
    { "realloc", 4u, WINDOWS_SHIM64_CRT_RVA_REALLOC },
    { "memcpy", 5u, WINDOWS_SHIM64_CRT_RVA_MEMCPY },
    { "memset", 6u, WINDOWS_SHIM64_CRT_RVA_MEMSET },
    { "memmove", 7u, WINDOWS_SHIM64_CRT_RVA_MEMMOVE },
    { "memcmp", 8u, WINDOWS_SHIM64_CRT_RVA_MEMCMP },
    { "strlen", 9u, WINDOWS_SHIM64_CRT_RVA_STRLEN },
    { "strcpy", 10u, WINDOWS_SHIM64_CRT_RVA_STRCPY },
    { "strcmp", 11u, WINDOWS_SHIM64_CRT_RVA_STRCMP },
    { "strcat", 12u, WINDOWS_SHIM64_CRT_RVA_STRCAT },
    { "fopen", 13u, WINDOWS_SHIM64_CRT_RVA_FOPEN },
    { "fclose", 14u, WINDOWS_SHIM64_CRT_RVA_FCLOSE },
    { "fread", 15u, WINDOWS_SHIM64_CRT_RVA_FREAD },
    { "fwrite", 16u, WINDOWS_SHIM64_CRT_RVA_FWRITE },
    { "fseek", 17u, WINDOWS_SHIM64_CRT_RVA_FSEEK },
    { "ftell", 18u, WINDOWS_SHIM64_CRT_RVA_FTELL },
    { "exit", 19u, WINDOWS_SHIM64_CRT_RVA_EXIT },
    { "__p___argc", 20u, WINDOWS_SHIM64_CRT_RVA_P_ARGC },
    { "__p___argv", 21u, WINDOWS_SHIM64_CRT_RVA_P_ARGV }
};

static u8 g_windows_shim64_kernel32_image[WINDOWS_SHIM64_KERNEL32_FILE_BYTES];
static u32 g_windows_shim64_kernel32_image_ready = 0u;
static u8 g_windows_shim64_crt_image[WINDOWS_SHIM64_CRT_FILE_BYTES];
static u32 g_windows_shim64_crt_image_ready = 0u;

static const u8 g_windows_shim64_ntdll_image[WINDOWS_SHIM64_NTDLL_FILE_BYTES] = {
    [0x000] = (u8)'M',
    [0x001] = (u8)'Z',
    [0x03C] = 0x80u,
    [0x080] = (u8)'P',
    [0x081] = (u8)'E',
    [0x084] = 0x64u,
    [0x085] = 0x86u,
    [0x086] = 0x02u,
    [0x094] = 0xF0u,
    [0x096] = 0x22u,
    [0x097] = 0x20u,
    [0x098] = 0x0Bu,
    [0x099] = 0x02u,
    [0x09A] = 0x01u,
    [0x09C] = 0x00u,
    [0x09D] = 0x02u,
    [0x0A0] = 0x00u,
    [0x0A1] = 0x02u,
    [0x0A8] = 0x00u,
    [0x0A9] = 0x10u,
    [0x0AC] = 0x00u,
    [0x0AD] = 0x10u,
    [0x0B2] = 0xC0u,
    [0x0B3] = 0x44u,
    [0x0B8] = 0x00u,
    [0x0B9] = 0x10u,
    [0x0BC] = 0x00u,
    [0x0BD] = 0x02u,
    [0x0D0] = 0x00u,
    [0x0D1] = 0x30u,
    [0x0D4] = 0x00u,
    [0x0D5] = 0x02u,
    [0x0DC] = 0x03u,
    [0x0E0] = 0x00u,
    [0x0E1] = 0x00u,
    [0x0E2] = 0x10u,
    [0x0E8] = 0x00u,
    [0x0E9] = 0x10u,
    [0x0F0] = 0x00u,
    [0x0F1] = 0x00u,
    [0x0F2] = 0x10u,
    [0x0F8] = 0x00u,
    [0x0F9] = 0x10u,
    [0x104] = 0x10u,
    [0x108] = 0x00u,
    [0x109] = 0x20u,
    [0x10C] = 0x00u,
    [0x10D] = 0x02u,
    [0x188] = (u8)'.',
    [0x189] = (u8)'t',
    [0x18A] = (u8)'e',
    [0x18B] = (u8)'x',
    [0x18C] = (u8)'t',
    [0x190] = 0x00u,
    [0x191] = 0x02u,
    [0x194] = 0x00u,
    [0x195] = 0x10u,
    [0x198] = 0x00u,
    [0x199] = 0x02u,
    [0x19C] = 0x00u,
    [0x19D] = 0x02u,
    [0x1AC] = 0x20u,
    [0x1AF] = 0x60u,
    [0x1B0] = (u8)'.',
    [0x1B1] = (u8)'r',
    [0x1B2] = (u8)'d',
    [0x1B3] = (u8)'a',
    [0x1B4] = (u8)'t',
    [0x1B5] = (u8)'a',
    [0x1B8] = 0x00u,
    [0x1B9] = 0x02u,
    [0x1BC] = 0x00u,
    [0x1BD] = 0x20u,
    [0x1C0] = 0x00u,
    [0x1C1] = 0x02u,
    [0x1C4] = 0x00u,
    [0x1C5] = 0x04u,
    [0x1D4] = 0x40u,
    [0x1D7] = 0x40u,

    [0x200] = 0x48u,
    [0x201] = 0x89u,
    [0x202] = 0xC8u,
    [0x203] = 0x48u,
    [0x204] = 0x81u,
    [0x205] = 0xF9u,
    [0x206] = 0x00u,
    [0x207] = 0x00u,
    [0x208] = 0x10u,
    [0x209] = 0x00u,
    [0x20A] = 0x73u,
    [0x20B] = 0x30u,
    [0x20C] = 0xBBu,
    [0x20D] = 0x32u,
    [0x20E] = 0x31u,
    [0x20F] = 0x45u,
    [0x210] = 0x50u,
    [0x211] = 0x45u,
    [0x212] = 0x31u,
    [0x213] = 0xC9u,
    [0x214] = 0x48u,
    [0x215] = 0x83u,
    [0x216] = 0xF9u,
    [0x217] = 0x01u,
    [0x218] = 0x75u,
    [0x219] = 0x16u,
    [0x21A] = 0x41u,
    [0x21B] = 0x83u,
    [0x21C] = 0xC9u,
    [0x21D] = 0x01u,
    [0x21E] = 0x48u,
    [0x21F] = 0x85u,
    [0x220] = 0xD2u,
    [0x221] = 0x74u,
    [0x222] = 0x0Du,
    [0x223] = 0x41u,
    [0x224] = 0x83u,
    [0x225] = 0xC9u,
    [0x226] = 0x02u,
    [0x227] = 0x4Du,
    [0x228] = 0x85u,
    [0x229] = 0xC0u,
    [0x22A] = 0x75u,
    [0x22B] = 0x04u,
    [0x22C] = 0x41u,
    [0x22D] = 0x83u,
    [0x22E] = 0xC9u,
    [0x22F] = 0x04u,
    [0x230] = 0x44u,
    [0x231] = 0x89u,
    [0x232] = 0xC9u,
    [0x233] = 0xB8u,
    [0x234] = (u8)(X64_SYSCALL_USERMODE_PROBE_EXIT & 0xFFu),
    [0x235] = (u8)((X64_SYSCALL_USERMODE_PROBE_EXIT >> 8) & 0xFFu),
    [0x236] = (u8)((X64_SYSCALL_USERMODE_PROBE_EXIT >> 16) & 0xFFu),
    [0x237] = (u8)((X64_SYSCALL_USERMODE_PROBE_EXIT >> 24) & 0xFFu),
    [0x238] = 0xCDu,
    [0x239] = 0x80u,
    [0x23A] = 0xEBu,
    [0x23B] = 0xFEu,
    [0x23C] = 0xFFu,
    [0x23D] = 0xE0u,

    [0x240] = 0xB8u,
    [0x241] = 0x02u,
    [0x242] = 0x00u,
    [0x243] = 0x00u,
    [0x244] = 0xC0u,
    [0x245] = 0xC3u,
    [0x260] = 0xB8u,
    [0x261] = 0x02u,
    [0x262] = 0x00u,
    [0x263] = 0x00u,
    [0x264] = 0xC0u,
    [0x265] = 0xC3u,
    [0x280] = 0xB8u,
    [0x281] = 0x02u,
    [0x282] = 0x00u,
    [0x283] = 0x00u,
    [0x284] = 0xC0u,
    [0x285] = 0xC3u,
    [0x2A0] = 0xB8u,
    [0x2A1] = 0x02u,
    [0x2A2] = 0x00u,
    [0x2A3] = 0x00u,
    [0x2A4] = 0xC0u,
    [0x2A5] = 0xC3u,
    [0x2C0] = 0xB8u,
    [0x2C1] = 0x02u,
    [0x2C2] = 0x00u,
    [0x2C3] = 0x00u,
    [0x2C4] = 0xC0u,
    [0x2C5] = 0xC3u,
    [0x2E0] = 0xB8u,
    [0x2E1] = 0x02u,
    [0x2E2] = 0x00u,
    [0x2E3] = 0x00u,
    [0x2E4] = 0xC0u,
    [0x2E5] = 0xC3u,
    [0x300] = 0xB8u,
    [0x301] = 0x02u,
    [0x302] = 0x00u,
    [0x303] = 0x00u,
    [0x304] = 0xC0u,
    [0x305] = 0xC3u,
    [0x320] = 0xB8u,
    [0x321] = 0x02u,
    [0x322] = 0x00u,
    [0x323] = 0x00u,
    [0x324] = 0xC0u,
    [0x325] = 0xC3u,

    [0x340] = 0x4Cu,
    [0x341] = 0x8Bu,
    [0x342] = 0xD1u,
    [0x343] = 0xB8u,
    [0x344] = (u8)(WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT & 0xFFu),
    [0x348] = 0x0Fu,
    [0x349] = 0x05u,
    [0x34A] = 0xC3u,
    [0x34C] = 0x4Cu,
    [0x34D] = 0x8Bu,
    [0x34E] = 0xD1u,
    [0x34F] = 0xB8u,
    [0x350] = (u8)(WINDOWS_ABI64_SYSCALL_NTREADFILE & 0xFFu),
    [0x354] = 0x0Fu,
    [0x355] = 0x05u,
    [0x356] = 0xC3u,
    [0x358] = 0x4Cu,
    [0x359] = 0x8Bu,
    [0x35A] = 0xD1u,
    [0x35B] = 0xB8u,
    [0x35C] = (u8)(WINDOWS_ABI64_SYSCALL_NTWRITEFILE & 0xFFu),
    [0x360] = 0x0Fu,
    [0x361] = 0x05u,
    [0x362] = 0xC3u,
    [0x364] = 0x4Cu,
    [0x365] = 0x8Bu,
    [0x366] = 0xD1u,
    [0x367] = 0xB8u,
    [0x368] = (u8)(WINDOWS_ABI64_SYSCALL_NTOPENKEY & 0xFFu),
    [0x36C] = 0x0Fu,
    [0x36D] = 0x05u,
    [0x36E] = 0xC3u,
    [0x370] = 0x4Cu,
    [0x371] = 0x8Bu,
    [0x372] = 0xD1u,
    [0x373] = 0xB8u,
    [0x374] = (u8)(WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY & 0xFFu),
    [0x378] = 0x0Fu,
    [0x379] = 0x05u,
    [0x37A] = 0xC3u,
    [0x37C] = 0x4Cu,
    [0x37D] = 0x8Bu,
    [0x37E] = 0xD1u,
    [0x37F] = 0xB8u,
    [0x380] = (u8)(WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY & 0xFFu),
    [0x384] = 0x0Fu,
    [0x385] = 0x05u,
    [0x386] = 0xC3u,
    [0x388] = 0x4Cu,
    [0x389] = 0x8Bu,
    [0x38A] = 0xD1u,
    [0x38B] = 0xB8u,
    [0x38C] = (u8)(WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS & 0xFFu),
    [0x390] = 0x0Fu,
    [0x391] = 0x05u,
    [0x392] = 0xC3u,
    [0x394] = 0x4Cu,
    [0x395] = 0x8Bu,
    [0x396] = 0xD1u,
    [0x397] = 0xB8u,
    [0x398] = (u8)(WINDOWS_ABI64_SYSCALL_NTCREATEKEY & 0xFFu),
    [0x39C] = 0x0Fu,
    [0x39D] = 0x05u,
    [0x39E] = 0xC3u,
    [0x3A0] = 0x4Cu,
    [0x3A1] = 0x8Bu,
    [0x3A2] = 0xD1u,
    [0x3A3] = 0xB8u,
    [0x3A4] = (u8)(WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY & 0xFFu),
    [0x3A8] = 0x0Fu,
    [0x3A9] = 0x05u,
    [0x3AA] = 0xC3u,
    [0x3AC] = 0x4Cu,
    [0x3AD] = 0x8Bu,
    [0x3AE] = 0xD1u,
    [0x3AF] = 0xB8u,
    [0x3B0] = (u8)(WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION & 0xFFu),
    [0x3B4] = 0x0Fu,
    [0x3B5] = 0x05u,
    [0x3B6] = 0xC3u,
    [0x3B8] = 0x4Cu,
    [0x3B9] = 0x8Bu,
    [0x3BA] = 0xD1u,
    [0x3BB] = 0xB8u,
    [0x3BC] = (u8)(WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY & 0xFFu),
    [0x3C0] = 0x0Fu,
    [0x3C1] = 0x05u,
    [0x3C2] = 0xC3u,
    [0x3C4] = 0x4Cu,
    [0x3C5] = 0x8Bu,
    [0x3C6] = 0xD1u,
    [0x3C7] = 0xB8u,
    [0x3C8] = (u8)(WINDOWS_ABI64_SYSCALL_NTCREATEFILE & 0xFFu),
    [0x3CC] = 0x0Fu,
    [0x3CD] = 0x05u,
    [0x3CE] = 0xC3u,
    [0x3D0] = 0x4Cu,
    [0x3D1] = 0x8Bu,
    [0x3D2] = 0xD1u,
    [0x3D3] = 0xB8u,
    [0x3D4] = (u8)(WINDOWS_ABI64_SYSCALL_NTCREATEEVENT & 0xFFu),
    [0x3D8] = 0x0Fu,
    [0x3D9] = 0x05u,
    [0x3DA] = 0xC3u,
    [0x3DC] = 0x4Cu,
    [0x3DD] = 0x8Bu,
    [0x3DE] = 0xD1u,
    [0x3DF] = 0xB8u,
    [0x3E0] = (u8)(WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT & 0xFFu),
    [0x3E4] = 0x0Fu,
    [0x3E5] = 0x05u,
    [0x3E6] = 0xC3u,
    [0x3E8] = 0x4Cu,
    [0x3E9] = 0x8Bu,
    [0x3EA] = 0xD1u,
    [0x3EB] = 0xB8u,
    [0x3EC] = (u8)(WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT & 0xFFu),
    [0x3F0] = 0x0Fu,
    [0x3F1] = 0x05u,
    [0x3F2] = 0xC3u,
    [0x3F4] = 0x4Cu,
    [0x3F5] = 0x8Bu,
    [0x3F6] = 0xD1u,
    [0x3F7] = 0xB8u,
    [0x3F8] = (u8)(WINDOWS_ABI64_SYSCALL_NTSETEVENT & 0xFFu),
    [0x3FC] = 0x0Fu,
    [0x3FD] = 0x05u,
    [0x3FE] = 0xC3u,

    [0x400] = (u8)'n',
    [0x401] = (u8)'t',
    [0x402] = (u8)'d',
    [0x403] = (u8)'l',
    [0x404] = (u8)'l',
    [0x405] = (u8)'.',
    [0x406] = (u8)'d',
    [0x407] = (u8)'l',
    [0x408] = (u8)'l',
    [0x40A] = (u8)'L',
    [0x40B] = (u8)'d',
    [0x40C] = (u8)'r',
    [0x40D] = (u8)'I',
    [0x40E] = (u8)'n',
    [0x40F] = (u8)'i',
    [0x410] = (u8)'t',
    [0x411] = (u8)'i',
    [0x412] = (u8)'a',
    [0x413] = (u8)'l',
    [0x414] = (u8)'i',
    [0x415] = (u8)'z',
    [0x416] = (u8)'e',
    [0x417] = (u8)'T',
    [0x418] = (u8)'h',
    [0x419] = (u8)'u',
    [0x41A] = (u8)'n',
    [0x41B] = (u8)'k',
    [0x41D] = (u8)'L',
    [0x41E] = (u8)'d',
    [0x41F] = (u8)'r',
    [0x420] = (u8)'L',
    [0x421] = (u8)'o',
    [0x422] = (u8)'a',
    [0x423] = (u8)'d',
    [0x424] = (u8)'D',
    [0x425] = (u8)'l',
    [0x426] = (u8)'l'
};

static u32 g_windows_shim64_ntdll_load_count = 0u;
static u32 g_windows_shim64_ntdll_denial_count = 0u;
static u32 g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_NONE;
static u64 g_windows_shim64_ntdll_last_base = 0ull;
static u32 g_windows_shim64_kernel32_load_count = 0u;
static u32 g_windows_shim64_kernel32_denial_count = 0u;
static u32 g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NONE;
static u64 g_windows_shim64_kernel32_last_base = 0ull;
static u32 g_windows_shim64_crt_load_count = 0u;
static u32 g_windows_shim64_crt_denial_count = 0u;
static u32 g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NONE;
static u64 g_windows_shim64_crt_last_base = 0ull;

static u8 windows_shim64_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u32 windows_shim64_name_equals(const char *left, const char *right)
{
    u32 index;

    if ((left == 0) || (right == 0))
    {
        return 0u;
    }

    for (index = 0u; index < 128u; ++index)
    {
        u8 l = windows_shim64_lower((u8)left[index]);
        u8 r = windows_shim64_lower((u8)right[index]);

        if (l != r)
        {
            return 0u;
        }
        if ((l == 0u) && (r == 0u))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 windows_shim64_checksum_step(u32 checksum, u32 value)
{
    checksum ^= value;
    checksum *= 16777619u;
    return checksum;
}

static u32 windows_shim64_checksum_bytes(const u8 *bytes, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (bytes == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = windows_shim64_checksum_step(checksum, (u32)bytes[index]);
    }

    return checksum;
}

static u32 windows_shim64_checksum_user(u64 address, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (address == 0ull)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum =
            windows_shim64_checksum_step(
                checksum,
                (u32)(*((volatile const u8 *)(u64)(address + (u64)index))));
    }

    return checksum;
}

static u32 windows_shim64_export_name_checksum(
    const windows_shim64_export_t *exports,
    u32 export_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (exports == 0)
    {
        return 0u;
    }

    for (index = 0u; index < export_count; ++index)
    {
        const char *name = exports[index].name;
        u32 name_index;

        for (name_index = 0u; name_index < 128u; ++name_index)
        {
            u8 value = (u8)name[name_index];

            checksum = windows_shim64_checksum_step(checksum, (u32)value);
            if (value == 0u)
            {
                break;
            }
        }
        checksum =
            windows_shim64_checksum_step(
                checksum,
                (u32)exports[index].ordinal);
        checksum =
            windows_shim64_checksum_step(
                checksum,
                exports[index].rva);
    }

    return checksum;
}

static u32 windows_shim64_name_checksum(void)
{
    return windows_shim64_export_name_checksum(
        g_windows_shim64_ntdll_exports,
        WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT);
}

static u32 windows_shim64_kernel32_name_checksum(void)
{
    return windows_shim64_export_name_checksum(
        g_windows_shim64_kernel32_exports,
        WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT);
}

static u32 windows_shim64_crt_name_checksum(void)
{
    return windows_shim64_export_name_checksum(
        g_windows_shim64_crt_exports,
        WINDOWS_SHIM64_CRT_SYMBOL_COUNT);
}

static void windows_shim64_put_u16(u8 *image, u32 image_size, u32 offset, u16 value)
{
    if ((image == 0) || ((offset + 2u) > image_size))
    {
        return;
    }

    image[offset] = (u8)(value & 0xFFu);
    image[offset + 1u] = (u8)((value >> 8) & 0xFFu);
}

static void windows_shim64_put_u32(u8 *image, u32 image_size, u32 offset, u32 value)
{
    if ((image == 0) || ((offset + 4u) > image_size))
    {
        return;
    }

    image[offset] = (u8)(value & 0xFFu);
    image[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    image[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    image[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static void windows_shim64_put_u64(u8 *image, u32 image_size, u32 offset, u64 value)
{
    u32 index;

    if ((image == 0) || ((offset + 8u) > image_size))
    {
        return;
    }

    for (index = 0u; index < 8u; ++index)
    {
        image[offset + index] = (u8)(value >> (index * 8u));
    }
}

static void windows_shim64_put_bytes(
    u8 *image,
    u32 image_size,
    u32 offset,
    const u8 *bytes,
    u32 byte_count)
{
    u32 index;

    if ((image == 0) || (bytes == 0) || ((offset + byte_count) > image_size))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        image[offset + index] = bytes[index];
    }
}

static u32 windows_shim64_kernel32_code_offset(u32 rva, u32 byte_count)
{
    u32 offset;

    if ((rva < WINDOWS_SHIM64_KERNEL32_TEXT_RVA)
        || (rva >= (WINDOWS_SHIM64_KERNEL32_TEXT_RVA + 0x00000400u))
        || ((rva + byte_count) < rva)
        || ((rva + byte_count) > (WINDOWS_SHIM64_KERNEL32_TEXT_RVA + 0x00000400u)))
    {
        return 0xFFFFFFFFu;
    }

    offset = 0x00000200u + (rva - WINDOWS_SHIM64_KERNEL32_TEXT_RVA);
    if ((offset + byte_count) > WINDOWS_SHIM64_KERNEL32_FILE_BYTES)
    {
        return 0xFFFFFFFFu;
    }

    return offset;
}

static void windows_shim64_put_kernel32_jump(u32 source_rva, u32 target_rva)
{
    u32 offset = windows_shim64_kernel32_code_offset(source_rva, 5u);
    u32 rel32;

    if (offset == 0xFFFFFFFFu)
    {
        return;
    }

    rel32 = target_rva - (source_rva + 5u);
    g_windows_shim64_kernel32_image[offset] = 0xE9u;
    windows_shim64_put_u32(
        g_windows_shim64_kernel32_image,
        WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
        offset + 1u,
        rel32);
}

static void windows_shim64_put_kernel32_helper(
    u32 rva,
    const u8 *bytes,
    u32 byte_count)
{
    u32 offset = windows_shim64_kernel32_code_offset(rva, byte_count);

    if (offset == 0xFFFFFFFFu)
    {
        return;
    }

    windows_shim64_put_bytes(
        g_windows_shim64_kernel32_image,
        WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
        offset,
        bytes,
        byte_count);
}

static u32 windows_shim64_put_ascii(
    u8 *image,
    u32 image_size,
    u32 offset,
    const char *text)
{
    u32 index;

    if ((image == 0) || (text == 0) || (offset >= image_size))
    {
        return offset;
    }

    for (index = 0u; (offset + index) < image_size; ++index)
    {
        image[offset + index] = (u8)text[index];
        if (text[index] == 0)
        {
            return offset + index + 1u;
        }
    }

    image[image_size - 1u] = 0u;
    return image_size;
}

static void windows_shim64_put_kernel32_stub(
    u32 rva,
    u32 ordinal)
{
    u32 offset;

    if ((rva < WINDOWS_SHIM64_KERNEL32_TEXT_RVA)
        || (rva >= (WINDOWS_SHIM64_KERNEL32_TEXT_RVA + 0x00000400u)))
    {
        return;
    }

    offset = windows_shim64_kernel32_code_offset(rva, 8u);
    if (offset == 0xFFFFFFFFu)
    {
        return;
    }

    if (rva == WINDOWS_SHIM64_KERNEL32_RVA_GET_STD_HANDLE)
    {
        windows_shim64_put_kernel32_jump(
            rva,
            WINDOWS_SHIM64_KERNEL32_RVA_HELPER_GET_STD_HANDLE);
        return;
    }
    if ((rva == WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_A)
        || (rva == WINDOWS_SHIM64_KERNEL32_RVA_WRITE_FILE))
    {
        windows_shim64_put_kernel32_jump(
            rva,
            WINDOWS_SHIM64_KERNEL32_RVA_HELPER_WRITE_FILE);
        return;
    }
    if (rva == WINDOWS_SHIM64_KERNEL32_RVA_GET_LAST_ERROR)
    {
        g_windows_shim64_kernel32_image[offset] = 0xB8u;
        g_windows_shim64_kernel32_image[offset + 1u] = 0x78u;
        g_windows_shim64_kernel32_image[offset + 2u] = 0x00u;
        g_windows_shim64_kernel32_image[offset + 3u] = 0x00u;
        g_windows_shim64_kernel32_image[offset + 4u] = 0x00u;
        g_windows_shim64_kernel32_image[offset + 5u] = 0xC3u;
        return;
    }
    if (rva == WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_PROCESS)
    {
        g_windows_shim64_kernel32_image[offset] = 0x48u;
        g_windows_shim64_kernel32_image[offset + 1u] = 0xC7u;
        g_windows_shim64_kernel32_image[offset + 2u] = 0xC0u;
        g_windows_shim64_kernel32_image[offset + 3u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 4u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 5u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 6u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 7u] = 0xC3u;
        return;
    }
    if (rva == WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_THREAD)
    {
        g_windows_shim64_kernel32_image[offset] = 0x48u;
        g_windows_shim64_kernel32_image[offset + 1u] = 0xC7u;
        g_windows_shim64_kernel32_image[offset + 2u] = 0xC0u;
        g_windows_shim64_kernel32_image[offset + 3u] = 0xFEu;
        g_windows_shim64_kernel32_image[offset + 4u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 5u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 6u] = 0xFFu;
        g_windows_shim64_kernel32_image[offset + 7u] = 0xC3u;
        return;
    }

    g_windows_shim64_kernel32_image[offset] = 0x31u;
    g_windows_shim64_kernel32_image[offset + 1u] = 0xC0u;
    g_windows_shim64_kernel32_image[offset + 2u] = 0xC3u;
    g_windows_shim64_kernel32_image[offset + 3u] = (u8)ordinal;
}

static void windows_shim64_build_kernel32_image(void)
{
    u32 index;
    u32 name_offset;

    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_FILE_BYTES; ++index)
    {
        g_windows_shim64_kernel32_image[index] = 0u;
    }

    g_windows_shim64_kernel32_image[0x000u] = (u8)'M';
    g_windows_shim64_kernel32_image[0x001u] = (u8)'Z';
    windows_shim64_put_u32(
        g_windows_shim64_kernel32_image,
        WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
        0x03Cu,
        0x00000080u);
    g_windows_shim64_kernel32_image[0x080u] = (u8)'P';
    g_windows_shim64_kernel32_image[0x081u] = (u8)'E';
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x084u, 0x8664u);
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x086u, 2u);
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x094u, 0x00F0u);
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x096u, 0x2022u);
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x098u, 0x020Bu);
    g_windows_shim64_kernel32_image[0x09Au] = 1u;
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x09Cu, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0A0u, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0A8u, WINDOWS_SHIM64_KERNEL32_RVA_EXIT_PROCESS);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0ACu, WINDOWS_SHIM64_KERNEL32_TEXT_RVA);
    windows_shim64_put_u64(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0B0u, WINDOWS_SHIM64_KERNEL32_DEFAULT_BASE);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0B8u, 0x00001000u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0BCu, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0D0u, WINDOWS_SHIM64_KERNEL32_IMAGE_BYTES);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0D4u, 0x00000200u);
    windows_shim64_put_u16(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0DCu, 3u);
    windows_shim64_put_u64(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0E0u, 0x0000000000100000ull);
    windows_shim64_put_u64(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0E8u, 0x0000000000001000ull);
    windows_shim64_put_u64(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0F0u, 0x0000000000100000ull);
    windows_shim64_put_u64(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x0F8u, 0x0000000000001000ull);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x104u, 16u);

    g_windows_shim64_kernel32_image[0x188u] = (u8)'.';
    g_windows_shim64_kernel32_image[0x189u] = (u8)'t';
    g_windows_shim64_kernel32_image[0x18Au] = (u8)'e';
    g_windows_shim64_kernel32_image[0x18Bu] = (u8)'x';
    g_windows_shim64_kernel32_image[0x18Cu] = (u8)'t';
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x190u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x194u, WINDOWS_SHIM64_KERNEL32_TEXT_RVA);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x198u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x19Cu, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1ACu, 0x60000020u);

    g_windows_shim64_kernel32_image[0x1B0u] = (u8)'.';
    g_windows_shim64_kernel32_image[0x1B1u] = (u8)'r';
    g_windows_shim64_kernel32_image[0x1B2u] = (u8)'d';
    g_windows_shim64_kernel32_image[0x1B3u] = (u8)'a';
    g_windows_shim64_kernel32_image[0x1B4u] = (u8)'t';
    g_windows_shim64_kernel32_image[0x1B5u] = (u8)'a';
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1B8u, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1BCu, WINDOWS_SHIM64_KERNEL32_RDATA_RVA);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1C0u, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1C4u, 0x00000600u);
    windows_shim64_put_u32(g_windows_shim64_kernel32_image, WINDOWS_SHIM64_KERNEL32_FILE_BYTES, 0x1D4u, 0x40000040u);

    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        windows_shim64_put_kernel32_stub(
            g_windows_shim64_kernel32_exports[index].rva,
            g_windows_shim64_kernel32_exports[index].ordinal);
    }
    windows_shim64_put_kernel32_helper(
        WINDOWS_SHIM64_KERNEL32_RVA_HELPER_GET_STD_HANDLE,
        g_windows_shim64_kernel32_get_std_handle_helper,
        (u32)sizeof(g_windows_shim64_kernel32_get_std_handle_helper));
    windows_shim64_put_kernel32_helper(
        WINDOWS_SHIM64_KERNEL32_RVA_HELPER_WRITE_FILE,
        g_windows_shim64_kernel32_write_file_helper,
        (u32)sizeof(g_windows_shim64_kernel32_write_file_helper));

    name_offset = 0x00000600u;
    name_offset =
        windows_shim64_put_ascii(
            g_windows_shim64_kernel32_image,
            WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
            name_offset,
            "kernel32.dll");
    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        name_offset =
            windows_shim64_put_ascii(
                g_windows_shim64_kernel32_image,
                WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
                name_offset,
                g_windows_shim64_kernel32_exports[index].name);
    }

    g_windows_shim64_kernel32_image_ready = 1u;
}

static void windows_shim64_put_crt_stub(
    u32 rva,
    u32 ordinal)
{
    u32 offset;

    if ((rva < WINDOWS_SHIM64_CRT_TEXT_RVA)
        || (rva >= (WINDOWS_SHIM64_CRT_TEXT_RVA + 0x00000400u)))
    {
        return;
    }

    offset = 0x00000200u + (rva - WINDOWS_SHIM64_CRT_TEXT_RVA);
    if ((offset + 8u) > WINDOWS_SHIM64_CRT_FILE_BYTES)
    {
        return;
    }

    if ((rva == WINDOWS_SHIM64_CRT_RVA_MEMCMP)
        || (rva == WINDOWS_SHIM64_CRT_RVA_STRCMP)
        || (rva == WINDOWS_SHIM64_CRT_RVA_FCLOSE)
        || (rva == WINDOWS_SHIM64_CRT_RVA_FSEEK)
        || (rva == WINDOWS_SHIM64_CRT_RVA_FTELL)
        || (rva == WINDOWS_SHIM64_CRT_RVA_PRINTF))
    {
        g_windows_shim64_crt_image[offset] = 0xB8u;
        g_windows_shim64_crt_image[offset + 1u] = 0xFFu;
        g_windows_shim64_crt_image[offset + 2u] = 0xFFu;
        g_windows_shim64_crt_image[offset + 3u] = 0xFFu;
        g_windows_shim64_crt_image[offset + 4u] = 0xFFu;
        g_windows_shim64_crt_image[offset + 5u] = 0xC3u;
        g_windows_shim64_crt_image[offset + 6u] = (u8)ordinal;
        return;
    }

    g_windows_shim64_crt_image[offset] = 0x31u;
    g_windows_shim64_crt_image[offset + 1u] = 0xC0u;
    g_windows_shim64_crt_image[offset + 2u] = 0xC3u;
    g_windows_shim64_crt_image[offset + 3u] = (u8)ordinal;
}

static void windows_shim64_build_crt_image(void)
{
    u32 index;
    u32 name_offset;

    for (index = 0u; index < WINDOWS_SHIM64_CRT_FILE_BYTES; ++index)
    {
        g_windows_shim64_crt_image[index] = 0u;
    }

    g_windows_shim64_crt_image[0x000u] = (u8)'M';
    g_windows_shim64_crt_image[0x001u] = (u8)'Z';
    windows_shim64_put_u32(
        g_windows_shim64_crt_image,
        WINDOWS_SHIM64_CRT_FILE_BYTES,
        0x03Cu,
        0x00000080u);
    g_windows_shim64_crt_image[0x080u] = (u8)'P';
    g_windows_shim64_crt_image[0x081u] = (u8)'E';
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x084u, 0x8664u);
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x086u, 2u);
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x094u, 0x00F0u);
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x096u, 0x2022u);
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x098u, 0x020Bu);
    g_windows_shim64_crt_image[0x09Au] = 1u;
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x09Cu, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0A0u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0A8u, WINDOWS_SHIM64_CRT_RVA_PRINTF);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0ACu, WINDOWS_SHIM64_CRT_TEXT_RVA);
    windows_shim64_put_u64(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0B0u, WINDOWS_SHIM64_CRT_DEFAULT_BASE);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0B8u, 0x00001000u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0BCu, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0D0u, WINDOWS_SHIM64_CRT_IMAGE_BYTES);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0D4u, 0x00000200u);
    windows_shim64_put_u16(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0DCu, 3u);
    windows_shim64_put_u64(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0E0u, 0x0000000000100000ull);
    windows_shim64_put_u64(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0E8u, 0x0000000000001000ull);
    windows_shim64_put_u64(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0F0u, 0x0000000000100000ull);
    windows_shim64_put_u64(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x0F8u, 0x0000000000001000ull);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x104u, 16u);

    g_windows_shim64_crt_image[0x188u] = (u8)'.';
    g_windows_shim64_crt_image[0x189u] = (u8)'t';
    g_windows_shim64_crt_image[0x18Au] = (u8)'e';
    g_windows_shim64_crt_image[0x18Bu] = (u8)'x';
    g_windows_shim64_crt_image[0x18Cu] = (u8)'t';
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x190u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x194u, WINDOWS_SHIM64_CRT_TEXT_RVA);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x198u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x19Cu, 0x00000200u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1ACu, 0x60000020u);

    g_windows_shim64_crt_image[0x1B0u] = (u8)'.';
    g_windows_shim64_crt_image[0x1B1u] = (u8)'r';
    g_windows_shim64_crt_image[0x1B2u] = (u8)'d';
    g_windows_shim64_crt_image[0x1B3u] = (u8)'a';
    g_windows_shim64_crt_image[0x1B4u] = (u8)'t';
    g_windows_shim64_crt_image[0x1B5u] = (u8)'a';
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1B8u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1BCu, WINDOWS_SHIM64_CRT_RDATA_RVA);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1C0u, 0x00000400u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1C4u, 0x00000600u);
    windows_shim64_put_u32(g_windows_shim64_crt_image, WINDOWS_SHIM64_CRT_FILE_BYTES, 0x1D4u, 0x40000040u);

    for (index = 0u; index < WINDOWS_SHIM64_CRT_SYMBOL_COUNT; ++index)
    {
        windows_shim64_put_crt_stub(
            g_windows_shim64_crt_exports[index].rva,
            g_windows_shim64_crt_exports[index].ordinal);
    }

    name_offset = 0x00000800u;
    name_offset =
        windows_shim64_put_ascii(
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES,
            name_offset,
            "msvcrt.dll");
    name_offset =
        windows_shim64_put_ascii(
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES,
            name_offset,
            "ucrtbase.dll");
    for (index = 0u; index < WINDOWS_SHIM64_CRT_SYMBOL_COUNT; ++index)
    {
        name_offset =
            windows_shim64_put_ascii(
                g_windows_shim64_crt_image,
                WINDOWS_SHIM64_CRT_FILE_BYTES,
                name_offset,
                g_windows_shim64_crt_exports[index].name);
    }

    g_windows_shim64_crt_image_ready = 1u;
}

static void windows_shim64_clear_result(windows_shim64_ntdll_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->image_base = 0ull;
    result->image_end = 0ull;
    result->ldr_initialize_thunk = 0ull;
    result->ldr_load_dll = 0ull;
    result->rtl_allocate_heap = 0ull;
    result->rtl_free_heap = 0ull;
    result->rtl_reallocate_heap = 0ull;
    result->rtl_create_heap = 0ull;
    result->rtl_user_thread_start = 0ull;
    result->ki_user_exception_dispatcher = 0ull;
    result->ntdll_def_window_proc_w = 0ull;
    result->file_bytes = 0u;
    result->image_bytes = 0u;
    result->section_count = 0u;
    result->mapped_count = 0u;
    result->symbol_count = 0u;
    result->image_checksum = 0u;
    result->text_checksum = 0u;
    result->rdata_checksum = 0u;
    result->name_checksum = 0u;
    result->text_protection = 0u;
    result->rdata_protection = 0u;
    result->context_stored = 0u;
    result->error = WINDOWS_SHIM64_ERROR_NONE;
}

static void windows_shim64_clear_kernel32_result(
    windows_shim64_kernel32_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->image_base = 0ull;
    result->image_end = 0ull;
    result->exit_process = 0ull;
    result->get_std_handle = 0ull;
    result->write_console_a = 0ull;
    result->write_console_w = 0ull;
    result->read_file = 0ull;
    result->write_file = 0ull;
    result->create_file_a = 0ull;
    result->create_file_w = 0ull;
    result->close_handle = 0ull;
    result->virtual_alloc = 0ull;
    result->virtual_free = 0ull;
    result->virtual_protect = 0ull;
    result->get_last_error = 0ull;
    result->set_last_error = 0ull;
    result->get_current_process = 0ull;
    result->get_current_thread = 0ull;
    result->get_system_info = 0ull;
    result->get_tick_count64 = 0ull;
    result->query_performance_counter = 0ull;
    result->get_process_heap = 0ull;
    result->heap_alloc = 0ull;
    result->heap_free = 0ull;
    result->heap_realloc = 0ull;
    result->load_library_a = 0ull;
    result->load_library_w = 0ull;
    result->get_proc_address = 0ull;
    result->file_bytes = 0u;
    result->image_bytes = 0u;
    result->section_count = 0u;
    result->mapped_count = 0u;
    result->symbol_count = 0u;
    result->image_checksum = 0u;
    result->text_checksum = 0u;
    result->rdata_checksum = 0u;
    result->name_checksum = 0u;
    result->text_protection = 0u;
    result->rdata_protection = 0u;
    result->ntdll_ready = 0u;
    result->nt_call_bridge_mask = 0u;
    result->live_stub_count = 0u;
    result->unavailable_stub_count = 0u;
    result->context_stored = 0u;
    result->error = WINDOWS_SHIM64_ERROR_NONE;
}

static void windows_shim64_clear_crt_result(
    windows_shim64_crt_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->image_base = 0ull;
    result->image_end = 0ull;
    result->printf_fn = 0ull;
    result->malloc_fn = 0ull;
    result->free_fn = 0ull;
    result->realloc_fn = 0ull;
    result->memcpy_fn = 0ull;
    result->memset_fn = 0ull;
    result->memmove_fn = 0ull;
    result->memcmp_fn = 0ull;
    result->strlen_fn = 0ull;
    result->strcpy_fn = 0ull;
    result->strcmp_fn = 0ull;
    result->strcat_fn = 0ull;
    result->fopen_fn = 0ull;
    result->fclose_fn = 0ull;
    result->fread_fn = 0ull;
    result->fwrite_fn = 0ull;
    result->fseek_fn = 0ull;
    result->ftell_fn = 0ull;
    result->exit_fn = 0ull;
    result->p_argc_fn = 0ull;
    result->p_argv_fn = 0ull;
    result->file_bytes = 0u;
    result->image_bytes = 0u;
    result->section_count = 0u;
    result->mapped_count = 0u;
    result->symbol_count = 0u;
    result->image_checksum = 0u;
    result->text_checksum = 0u;
    result->rdata_checksum = 0u;
    result->name_checksum = 0u;
    result->text_protection = 0u;
    result->rdata_protection = 0u;
    result->kernel32_ready = 0u;
    result->kernel32_export_mask = 0u;
    result->live_stub_count = 0u;
    result->unavailable_stub_count = 0u;
    result->context_stored = 0u;
    result->error = WINDOWS_SHIM64_ERROR_NONE;
}

static void windows_shim64_set_error(
    windows_shim64_ntdll_result_t *result,
    u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
    g_windows_shim64_ntdll_last_error = error;
    if (error != WINDOWS_SHIM64_ERROR_NONE)
    {
        ++g_windows_shim64_ntdll_denial_count;
    }
}

static void windows_shim64_set_kernel32_error(
    windows_shim64_kernel32_result_t *result,
    u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
    g_windows_shim64_kernel32_last_error = error;
    if (error != WINDOWS_SHIM64_ERROR_NONE)
    {
        ++g_windows_shim64_kernel32_denial_count;
    }
}

static void windows_shim64_set_crt_error(
    windows_shim64_crt_result_t *result,
    u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
    g_windows_shim64_crt_last_error = error;
    if (error != WINDOWS_SHIM64_ERROR_NONE)
    {
        ++g_windows_shim64_crt_denial_count;
    }
}

void windows_shim64_init(void)
{
    g_windows_shim64_ntdll_load_count = 0u;
    g_windows_shim64_ntdll_denial_count = 0u;
    g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_NONE;
    g_windows_shim64_ntdll_last_base = 0ull;
    g_windows_shim64_kernel32_load_count = 0u;
    g_windows_shim64_kernel32_denial_count = 0u;
    g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NONE;
    g_windows_shim64_kernel32_last_base = 0ull;
    g_windows_shim64_crt_load_count = 0u;
    g_windows_shim64_crt_denial_count = 0u;
    g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NONE;
    g_windows_shim64_crt_last_base = 0ull;
    windows_shim64_build_kernel32_image();
    windows_shim64_build_crt_image();
}

u32 windows_shim64_load_ntdll(
    u32 pid,
    u64 image_base,
    windows_shim64_ntdll_result_t *out_result)
{
    pe64_header_t header;
    pe64_section_t sections[PE64_MAX_SECTIONS];
    pe64_section_summary_t section_summary;
    pe64_map_result_t map_result;
    persona_context_t *context;
    u32 parse_header;
    u32 parse_sections;
    u32 map_sections;

    windows_shim64_clear_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_NULL);
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_PERSONA);
        return WINDOWS_SHIM64_DENIED;
    }

    if (image_base == 0ull)
    {
        image_base = WINDOWS_SHIM64_NTDLL_DEFAULT_BASE;
    }
    if (((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((image_base + (u64)WINDOWS_SHIM64_NTDLL_IMAGE_BYTES) < image_base))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_BASE);
        return WINDOWS_SHIM64_DENIED;
    }

    if ((vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_NTDLL_TEXT_RVA) != 0)
        || (vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_NTDLL_RDATA_RVA) != 0))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_ALREADY_MAPPED);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_header =
        pe64_parse_header(
            g_windows_shim64_ntdll_image,
            WINDOWS_SHIM64_NTDLL_FILE_BYTES,
            &header);
    if ((parse_header != PE64_OK)
        || (header.error != PE64_ERROR_NONE)
        || ((header.characteristics & PE64_IMAGE_FILE_DLL) == 0u))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_HEADER);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_sections =
        pe64_parse_sections(
            g_windows_shim64_ntdll_image,
            WINDOWS_SHIM64_NTDLL_FILE_BYTES,
            &header,
            sections,
            PE64_MAX_SECTIONS,
            &section_summary);
    if ((parse_sections != PE64_OK)
        || (section_summary.error != PE64_ERROR_NONE)
        || (section_summary.section_count != 2u))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_SECTION);
        return WINDOWS_SHIM64_DENIED;
    }

    map_sections =
        pe64_map_sections(
            pid,
            &header,
            sections,
            section_summary.section_count,
            g_windows_shim64_ntdll_image,
            WINDOWS_SHIM64_NTDLL_FILE_BYTES,
            image_base,
            &map_result);
    if ((map_sections != PE64_OK)
        || (map_result.error != PE64_ERROR_NONE)
        || (map_result.mapped_count != 2u))
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_MAP);
        return WINDOWS_SHIM64_DENIED;
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)WINDOWS_SHIM64_NTDLL_IMAGE_BYTES;
    out_result->ldr_initialize_thunk =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_LDR_INITIALIZE_THUNK;
    out_result->ldr_load_dll =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_LDR_LOAD_DLL;
    out_result->rtl_allocate_heap =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_RTL_ALLOCATE_HEAP;
    out_result->rtl_free_heap =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_RTL_FREE_HEAP;
    out_result->rtl_reallocate_heap =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_RTL_REALLOCATE_HEAP;
    out_result->rtl_create_heap =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_RTL_CREATE_HEAP;
    out_result->rtl_user_thread_start =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_RTL_USER_THREAD_START;
    out_result->ki_user_exception_dispatcher =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_KI_USER_EXCEPTION_DISPATCHER;
    out_result->ntdll_def_window_proc_w =
        image_base + (u64)WINDOWS_SHIM64_NTDLL_RVA_NTDLL_DEF_WINDOW_PROC_W;
    out_result->file_bytes = WINDOWS_SHIM64_NTDLL_FILE_BYTES;
    out_result->image_bytes = header.size_of_image;
    out_result->section_count = section_summary.section_count;
    out_result->mapped_count = map_result.mapped_count;
    out_result->symbol_count = WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT;
    out_result->image_checksum =
        windows_shim64_checksum_bytes(
            g_windows_shim64_ntdll_image,
            WINDOWS_SHIM64_NTDLL_FILE_BYTES);
    out_result->text_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_NTDLL_TEXT_RVA,
            WINDOWS_SHIM64_NTDLL_PAGE_BYTES);
    out_result->rdata_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_NTDLL_RDATA_RVA,
            WINDOWS_SHIM64_NTDLL_PAGE_BYTES);
    out_result->name_checksum = windows_shim64_name_checksum();
    out_result->text_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_NTDLL_TEXT_RVA);
    out_result->rdata_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_NTDLL_RDATA_RVA);

    context->windows_ntdll_base = image_base;
    context->windows_ntdll_ldr_initialize_thunk = out_result->ldr_initialize_thunk;
    context->windows_ntdll_symbol_count = WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT;
    context->windows_ntdll_checksum = out_result->image_checksum;
    out_result->context_stored =
        ((context->windows_ntdll_base == image_base)
            && (context->windows_ntdll_ldr_initialize_thunk
                == out_result->ldr_initialize_thunk)
            && (context->windows_ntdll_symbol_count
                == WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT)
            && (context->windows_ntdll_checksum == out_result->image_checksum))
            ? 1u
            : 0u;

    if (out_result->context_stored == 0u)
    {
        windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_CONTEXT);
        return WINDOWS_SHIM64_DENIED;
    }

    ++g_windows_shim64_ntdll_load_count;
    g_windows_shim64_ntdll_last_base = image_base;
    windows_shim64_set_error(out_result, WINDOWS_SHIM64_ERROR_NONE);
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_load_kernel32(
    u32 pid,
    u64 image_base,
    windows_shim64_kernel32_result_t *out_result)
{
    pe64_header_t header;
    pe64_section_t sections[PE64_MAX_SECTIONS];
    pe64_section_summary_t section_summary;
    pe64_map_result_t map_result;
    persona_context_t *context;
    u32 parse_header;
    u32 parse_sections;
    u32 map_sections;

    windows_shim64_clear_kernel32_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_NULL);
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_PERSONA);
        return WINDOWS_SHIM64_DENIED;
    }

    if ((context->windows_ntdll_base == 0ull)
        || (context->windows_ntdll_ldr_initialize_thunk == 0ull)
        || (context->windows_ntdll_symbol_count != WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_DEPENDENCY);
        return WINDOWS_SHIM64_DENIED;
    }

    if (g_windows_shim64_kernel32_image_ready == 0u)
    {
        windows_shim64_build_kernel32_image();
    }

    if (image_base == 0ull)
    {
        image_base = WINDOWS_SHIM64_KERNEL32_DEFAULT_BASE;
    }
    if (((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((image_base + (u64)WINDOWS_SHIM64_KERNEL32_IMAGE_BYTES) < image_base))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_BASE);
        return WINDOWS_SHIM64_DENIED;
    }

    if ((vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_KERNEL32_TEXT_RVA) != 0)
        || (vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_KERNEL32_RDATA_RVA) != 0))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_ALREADY_MAPPED);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_header =
        pe64_parse_header(
            g_windows_shim64_kernel32_image,
            WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
            &header);
    if ((parse_header != PE64_OK)
        || (header.error != PE64_ERROR_NONE)
        || ((header.characteristics & PE64_IMAGE_FILE_DLL) == 0u))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_HEADER);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_sections =
        pe64_parse_sections(
            g_windows_shim64_kernel32_image,
            WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
            &header,
            sections,
            PE64_MAX_SECTIONS,
            &section_summary);
    if ((parse_sections != PE64_OK)
        || (section_summary.error != PE64_ERROR_NONE)
        || (section_summary.section_count != 2u))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_SECTION);
        return WINDOWS_SHIM64_DENIED;
    }

    map_sections =
        pe64_map_sections(
            pid,
            &header,
            sections,
            section_summary.section_count,
            g_windows_shim64_kernel32_image,
            WINDOWS_SHIM64_KERNEL32_FILE_BYTES,
            image_base,
            &map_result);
    if ((map_sections != PE64_OK)
        || (map_result.error != PE64_ERROR_NONE)
        || (map_result.mapped_count != 2u))
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_MAP);
        return WINDOWS_SHIM64_DENIED;
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)WINDOWS_SHIM64_KERNEL32_IMAGE_BYTES;
    out_result->exit_process = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_EXIT_PROCESS;
    out_result->get_std_handle = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_STD_HANDLE;
    out_result->write_console_a = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_A;
    out_result->write_console_w = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_W;
    out_result->read_file = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_READ_FILE;
    out_result->write_file = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_WRITE_FILE;
    out_result->create_file_a = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_A;
    out_result->create_file_w = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_W;
    out_result->close_handle = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_CLOSE_HANDLE;
    out_result->virtual_alloc = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_ALLOC;
    out_result->virtual_free = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_FREE;
    out_result->virtual_protect = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_PROTECT;
    out_result->get_last_error = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_LAST_ERROR;
    out_result->set_last_error = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_SET_LAST_ERROR;
    out_result->get_current_process = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_PROCESS;
    out_result->get_current_thread = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_THREAD;
    out_result->get_system_info = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_SYSTEM_INFO;
    out_result->get_tick_count64 = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_TICK_COUNT64;
    out_result->query_performance_counter = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_QUERY_PERFORMANCE_COUNTER;
    out_result->get_process_heap = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_PROCESS_HEAP;
    out_result->heap_alloc = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_HEAP_ALLOC;
    out_result->heap_free = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_HEAP_FREE;
    out_result->heap_realloc = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_HEAP_REALLOC;
    out_result->load_library_a = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_A;
    out_result->load_library_w = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_W;
    out_result->get_proc_address = image_base + (u64)WINDOWS_SHIM64_KERNEL32_RVA_GET_PROC_ADDRESS;
    out_result->file_bytes = WINDOWS_SHIM64_KERNEL32_FILE_BYTES;
    out_result->image_bytes = header.size_of_image;
    out_result->section_count = section_summary.section_count;
    out_result->mapped_count = map_result.mapped_count;
    out_result->symbol_count = WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT;
    out_result->image_checksum =
        windows_shim64_checksum_bytes(
            g_windows_shim64_kernel32_image,
            WINDOWS_SHIM64_KERNEL32_FILE_BYTES);
    out_result->text_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_KERNEL32_TEXT_RVA,
            WINDOWS_SHIM64_KERNEL32_PAGE_BYTES);
    out_result->rdata_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_KERNEL32_RDATA_RVA,
            WINDOWS_SHIM64_KERNEL32_PAGE_BYTES);
    out_result->name_checksum = windows_shim64_kernel32_name_checksum();
    out_result->text_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_KERNEL32_TEXT_RVA);
    out_result->rdata_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_KERNEL32_RDATA_RVA);
    out_result->ntdll_ready = 1u;
    out_result->nt_call_bridge_mask =
        ((windows_abi64_write_entry_installed() != 0u) ? 0x00000001u : 0u)
            | ((windows_abi64_read_entry_installed() != 0u) ? 0x00000002u : 0u)
            | ((windows_abi64_create_entry_installed() != 0u) ? 0x00000004u : 0u)
            | ((windows_abi64_allocate_entry_installed() != 0u) ? 0x00000008u : 0u)
            | ((windows_abi64_free_entry_installed() != 0u) ? 0x00000010u : 0u)
            | ((windows_abi64_protect_entry_installed() != 0u) ? 0x00000020u : 0u)
            | ((windows_abi64_query_system_entry_installed() != 0u) ? 0x00000040u : 0u)
            | ((windows_abi64_query_process_entry_installed() != 0u) ? 0x00000080u : 0u);
    out_result->live_stub_count = WINDOWS_SHIM64_KERNEL32_LIVE_SYMBOL_COUNT;
    out_result->unavailable_stub_count = WINDOWS_SHIM64_KERNEL32_UNAVAILABLE_SYMBOL_COUNT;

    context->windows_kernel32_base = image_base;
    context->windows_kernel32_write_console_a = out_result->write_console_a;
    context->windows_kernel32_symbol_count = WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT;
    context->windows_kernel32_checksum = out_result->image_checksum;
    out_result->context_stored =
        ((context->windows_kernel32_base == image_base)
            && (context->windows_kernel32_write_console_a == out_result->write_console_a)
            && (context->windows_kernel32_symbol_count
                == WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT)
            && (context->windows_kernel32_checksum == out_result->image_checksum))
            ? 1u
            : 0u;

    if (out_result->context_stored == 0u)
    {
        windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_CONTEXT);
        return WINDOWS_SHIM64_DENIED;
    }

    ++g_windows_shim64_kernel32_load_count;
    g_windows_shim64_kernel32_last_base = image_base;
    windows_shim64_set_kernel32_error(out_result, WINDOWS_SHIM64_ERROR_NONE);
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_load_crt(
    u32 pid,
    u64 image_base,
    windows_shim64_crt_result_t *out_result)
{
    pe64_header_t header;
    pe64_section_t sections[PE64_MAX_SECTIONS];
    pe64_section_summary_t section_summary;
    pe64_map_result_t map_result;
    persona_context_t *context;
    u32 parse_header;
    u32 parse_sections;
    u32 map_sections;

    windows_shim64_clear_crt_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_NULL);
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_PERSONA);
        return WINDOWS_SHIM64_DENIED;
    }

    if ((context->windows_ntdll_base == 0ull)
        || (context->windows_kernel32_base == 0ull)
        || (context->windows_kernel32_write_console_a == 0ull)
        || (context->windows_kernel32_symbol_count != WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_DEPENDENCY);
        return WINDOWS_SHIM64_DENIED;
    }

    if (g_windows_shim64_crt_image_ready == 0u)
    {
        windows_shim64_build_crt_image();
    }

    if (image_base == 0ull)
    {
        image_base = WINDOWS_SHIM64_CRT_DEFAULT_BASE;
    }
    if (((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((image_base + (u64)WINDOWS_SHIM64_CRT_IMAGE_BYTES) < image_base))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_BASE);
        return WINDOWS_SHIM64_DENIED;
    }

    if ((vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_CRT_TEXT_RVA) != 0)
        || (vma64_find(pid, image_base + (u64)WINDOWS_SHIM64_CRT_RDATA_RVA) != 0))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_ALREADY_MAPPED);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_header =
        pe64_parse_header(
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES,
            &header);
    if ((parse_header != PE64_OK)
        || (header.error != PE64_ERROR_NONE)
        || ((header.characteristics & PE64_IMAGE_FILE_DLL) == 0u))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_HEADER);
        return WINDOWS_SHIM64_DENIED;
    }

    parse_sections =
        pe64_parse_sections(
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES,
            &header,
            sections,
            PE64_MAX_SECTIONS,
            &section_summary);
    if ((parse_sections != PE64_OK)
        || (section_summary.error != PE64_ERROR_NONE)
        || (section_summary.section_count != 2u))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_SECTION);
        return WINDOWS_SHIM64_DENIED;
    }

    map_sections =
        pe64_map_sections(
            pid,
            &header,
            sections,
            section_summary.section_count,
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES,
            image_base,
            &map_result);
    if ((map_sections != PE64_OK)
        || (map_result.error != PE64_ERROR_NONE)
        || (map_result.mapped_count != 2u))
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_MAP);
        return WINDOWS_SHIM64_DENIED;
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)WINDOWS_SHIM64_CRT_IMAGE_BYTES;
    out_result->printf_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_PRINTF;
    out_result->malloc_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_MALLOC;
    out_result->free_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FREE;
    out_result->realloc_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_REALLOC;
    out_result->memcpy_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_MEMCPY;
    out_result->memset_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_MEMSET;
    out_result->memmove_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_MEMMOVE;
    out_result->memcmp_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_MEMCMP;
    out_result->strlen_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_STRLEN;
    out_result->strcpy_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_STRCPY;
    out_result->strcmp_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_STRCMP;
    out_result->strcat_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_STRCAT;
    out_result->fopen_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FOPEN;
    out_result->fclose_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FCLOSE;
    out_result->fread_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FREAD;
    out_result->fwrite_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FWRITE;
    out_result->fseek_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FSEEK;
    out_result->ftell_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_FTELL;
    out_result->exit_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_EXIT;
    out_result->p_argc_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_P_ARGC;
    out_result->p_argv_fn = image_base + (u64)WINDOWS_SHIM64_CRT_RVA_P_ARGV;
    out_result->file_bytes = WINDOWS_SHIM64_CRT_FILE_BYTES;
    out_result->image_bytes = header.size_of_image;
    out_result->section_count = section_summary.section_count;
    out_result->mapped_count = map_result.mapped_count;
    out_result->symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    out_result->image_checksum =
        windows_shim64_checksum_bytes(
            g_windows_shim64_crt_image,
            WINDOWS_SHIM64_CRT_FILE_BYTES);
    out_result->text_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_CRT_TEXT_RVA,
            WINDOWS_SHIM64_CRT_PAGE_BYTES);
    out_result->rdata_checksum =
        windows_shim64_checksum_user(
            image_base + (u64)WINDOWS_SHIM64_CRT_RDATA_RVA,
            WINDOWS_SHIM64_CRT_PAGE_BYTES);
    out_result->name_checksum = windows_shim64_crt_name_checksum();
    out_result->text_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_CRT_TEXT_RVA);
    out_result->rdata_protection =
        paging64_user_page_protection(image_base + (u64)WINDOWS_SHIM64_CRT_RDATA_RVA);
    out_result->kernel32_ready = 1u;
    out_result->kernel32_export_mask =
        ((windows_shim64_kernel32_export(pid, "ExitProcess") != 0ull) ? 0x00000001u : 0u)
            | ((windows_shim64_kernel32_export(pid, "WriteConsoleA") != 0ull) ? 0x00000002u : 0u)
            | ((windows_shim64_kernel32_export(pid, "HeapAlloc") != 0ull) ? 0x00000004u : 0u)
            | ((windows_shim64_kernel32_export(pid, "HeapFree") != 0ull) ? 0x00000008u : 0u)
            | ((windows_shim64_kernel32_export(pid, "HeapReAlloc") != 0ull) ? 0x00000010u : 0u)
            | ((windows_shim64_kernel32_export(pid, "CreateFileA") != 0ull) ? 0x00000020u : 0u)
            | ((windows_shim64_kernel32_export(pid, "ReadFile") != 0ull) ? 0x00000040u : 0u)
            | ((windows_shim64_kernel32_export(pid, "WriteFile") != 0ull) ? 0x00000080u : 0u);
    out_result->live_stub_count = 0u;
    out_result->unavailable_stub_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;

    context->windows_crt_base = image_base;
    context->windows_crt_printf = out_result->printf_fn;
    context->windows_crt_symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    context->windows_crt_checksum = out_result->image_checksum;
    out_result->context_stored =
        ((context->windows_crt_base == image_base)
            && (context->windows_crt_printf == out_result->printf_fn)
            && (context->windows_crt_symbol_count == WINDOWS_SHIM64_CRT_SYMBOL_COUNT)
            && (context->windows_crt_checksum == out_result->image_checksum))
            ? 1u
            : 0u;

    if (out_result->context_stored == 0u)
    {
        windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_CONTEXT);
        return WINDOWS_SHIM64_DENIED;
    }

    ++g_windows_shim64_crt_load_count;
    g_windows_shim64_crt_last_base = image_base;
    windows_shim64_set_crt_error(out_result, WINDOWS_SHIM64_ERROR_NONE);
    return WINDOWS_SHIM64_OK;
}

u64 windows_shim64_ntdll_export(u32 pid, const char *name)
{
    persona_context_t *context;
    u32 index;

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_ntdll_base == 0ull)
        || (name == 0))
    {
        g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
        return 0ull;
    }

    for (index = 0u; index < WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT; ++index)
    {
        if (windows_shim64_name_equals(name, g_windows_shim64_ntdll_exports[index].name) != 0u)
        {
            g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_NONE;
            return context->windows_ntdll_base
                + (u64)g_windows_shim64_ntdll_exports[index].rva;
        }
    }

    g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
    return 0ull;
}

u64 windows_shim64_kernel32_export(u32 pid, const char *name)
{
    persona_context_t *context;
    u32 index;

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_kernel32_base == 0ull)
        || (name == 0))
    {
        g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
        return 0ull;
    }

    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        if (windows_shim64_name_equals(name, g_windows_shim64_kernel32_exports[index].name) != 0u)
        {
            g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NONE;
            return context->windows_kernel32_base
                + (u64)g_windows_shim64_kernel32_exports[index].rva;
        }
    }

    g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
    return 0ull;
}

u64 windows_shim64_crt_export(u32 pid, const char *name)
{
    persona_context_t *context;
    u32 index;

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_crt_base == 0ull)
        || (name == 0))
    {
        g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
        return 0ull;
    }

    for (index = 0u; index < WINDOWS_SHIM64_CRT_SYMBOL_COUNT; ++index)
    {
        if (windows_shim64_name_equals(name, g_windows_shim64_crt_exports[index].name) != 0u)
        {
            g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NONE;
            return context->windows_crt_base
                + (u64)g_windows_shim64_crt_exports[index].rva;
        }
    }

    g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_EXPORT;
    return 0ull;
}

u32 windows_shim64_ntdll_registry(u32 pid, windows_shim64_registry_t *out_registry)
{
    persona_context_t *context;
    u32 index;

    if (out_registry == 0)
    {
        g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_NULL;
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_ntdll_base == 0ull))
    {
        g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_PERSONA;
        return WINDOWS_SHIM64_DENIED;
    }

    for (index = 0u; index < WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT; ++index)
    {
        out_registry->ntdll_symbols[index].name =
            g_windows_shim64_ntdll_exports[index].name;
        out_registry->ntdll_symbols[index].ordinal =
            g_windows_shim64_ntdll_exports[index].ordinal;
        out_registry->ntdll_symbols[index].address =
            context->windows_ntdll_base
                + (u64)g_windows_shim64_ntdll_exports[index].rva;
    }

    out_registry->libraries[0].dll_name = "ntdll.dll";
    out_registry->libraries[0].symbols = out_registry->ntdll_symbols;
    out_registry->libraries[0].symbol_count = WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT;
    out_registry->registry.libraries = out_registry->libraries;
    out_registry->registry.library_count = WINDOWS_SHIM64_NTDLL_LIBRARY_COUNT;
    g_windows_shim64_ntdll_last_error = WINDOWS_SHIM64_ERROR_NONE;
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_kernel32_registry(u32 pid, windows_shim64_registry_t *out_registry)
{
    persona_context_t *context;
    u32 index;

    if (out_registry == 0)
    {
        g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NULL;
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_kernel32_base == 0ull))
    {
        g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_PERSONA;
        return WINDOWS_SHIM64_DENIED;
    }

    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        out_registry->kernel32_symbols[index].name =
            g_windows_shim64_kernel32_exports[index].name;
        out_registry->kernel32_symbols[index].ordinal =
            g_windows_shim64_kernel32_exports[index].ordinal;
        out_registry->kernel32_symbols[index].address =
            context->windows_kernel32_base
                + (u64)g_windows_shim64_kernel32_exports[index].rva;
    }

    out_registry->libraries[0].dll_name = "kernel32.dll";
    out_registry->libraries[0].symbols = out_registry->kernel32_symbols;
    out_registry->libraries[0].symbol_count = WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT;
    out_registry->registry.libraries = out_registry->libraries;
    out_registry->registry.library_count = WINDOWS_SHIM64_KERNEL32_LIBRARY_COUNT;
    g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NONE;
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_combined_registry(u32 pid, windows_shim64_registry_t *out_registry)
{
    persona_context_t *context;
    u32 index;

    if (out_registry == 0)
    {
        g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NULL;
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_ntdll_base == 0ull)
        || (context->windows_kernel32_base == 0ull))
    {
        g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_PERSONA;
        return WINDOWS_SHIM64_DENIED;
    }

    for (index = 0u; index < WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT; ++index)
    {
        out_registry->ntdll_symbols[index].name =
            g_windows_shim64_ntdll_exports[index].name;
        out_registry->ntdll_symbols[index].ordinal =
            g_windows_shim64_ntdll_exports[index].ordinal;
        out_registry->ntdll_symbols[index].address =
            context->windows_ntdll_base
                + (u64)g_windows_shim64_ntdll_exports[index].rva;
    }
    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        out_registry->kernel32_symbols[index].name =
            g_windows_shim64_kernel32_exports[index].name;
        out_registry->kernel32_symbols[index].ordinal =
            g_windows_shim64_kernel32_exports[index].ordinal;
        out_registry->kernel32_symbols[index].address =
            context->windows_kernel32_base
                + (u64)g_windows_shim64_kernel32_exports[index].rva;
    }

    out_registry->libraries[0].dll_name = "ntdll.dll";
    out_registry->libraries[0].symbols = out_registry->ntdll_symbols;
    out_registry->libraries[0].symbol_count = WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT;
    out_registry->libraries[1].dll_name = "kernel32.dll";
    out_registry->libraries[1].symbols = out_registry->kernel32_symbols;
    out_registry->libraries[1].symbol_count = WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT;
    out_registry->registry.libraries = out_registry->libraries;
    out_registry->registry.library_count = WINDOWS_SHIM64_COMBINED_LIBRARY_COUNT;
    g_windows_shim64_kernel32_last_error = WINDOWS_SHIM64_ERROR_NONE;
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_crt_registry(u32 pid, windows_shim64_registry_t *out_registry)
{
    persona_context_t *context;
    u32 index;

    if (out_registry == 0)
    {
        g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NULL;
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_crt_base == 0ull))
    {
        g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_PERSONA;
        return WINDOWS_SHIM64_DENIED;
    }

    for (index = 0u; index < WINDOWS_SHIM64_CRT_SYMBOL_COUNT; ++index)
    {
        out_registry->crt_symbols[index].name =
            g_windows_shim64_crt_exports[index].name;
        out_registry->crt_symbols[index].ordinal =
            g_windows_shim64_crt_exports[index].ordinal;
        out_registry->crt_symbols[index].address =
            context->windows_crt_base
                + (u64)g_windows_shim64_crt_exports[index].rva;
    }

    out_registry->libraries[0].dll_name = "msvcrt.dll";
    out_registry->libraries[0].symbols = out_registry->crt_symbols;
    out_registry->libraries[0].symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    out_registry->libraries[1].dll_name = "ucrtbase.dll";
    out_registry->libraries[1].symbols = out_registry->crt_symbols;
    out_registry->libraries[1].symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    out_registry->registry.libraries = out_registry->libraries;
    out_registry->registry.library_count = WINDOWS_SHIM64_CRT_LIBRARY_COUNT;
    g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NONE;
    return WINDOWS_SHIM64_OK;
}

u32 windows_shim64_full_registry(u32 pid, windows_shim64_registry_t *out_registry)
{
    persona_context_t *context;
    u32 index;

    if (out_registry == 0)
    {
        g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NULL;
        return WINDOWS_SHIM64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_ntdll_base == 0ull)
        || (context->windows_kernel32_base == 0ull)
        || (context->windows_crt_base == 0ull))
    {
        g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_PERSONA;
        return WINDOWS_SHIM64_DENIED;
    }

    for (index = 0u; index < WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT; ++index)
    {
        out_registry->ntdll_symbols[index].name =
            g_windows_shim64_ntdll_exports[index].name;
        out_registry->ntdll_symbols[index].ordinal =
            g_windows_shim64_ntdll_exports[index].ordinal;
        out_registry->ntdll_symbols[index].address =
            context->windows_ntdll_base
                + (u64)g_windows_shim64_ntdll_exports[index].rva;
    }
    for (index = 0u; index < WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT; ++index)
    {
        out_registry->kernel32_symbols[index].name =
            g_windows_shim64_kernel32_exports[index].name;
        out_registry->kernel32_symbols[index].ordinal =
            g_windows_shim64_kernel32_exports[index].ordinal;
        out_registry->kernel32_symbols[index].address =
            context->windows_kernel32_base
                + (u64)g_windows_shim64_kernel32_exports[index].rva;
    }
    for (index = 0u; index < WINDOWS_SHIM64_CRT_SYMBOL_COUNT; ++index)
    {
        out_registry->crt_symbols[index].name =
            g_windows_shim64_crt_exports[index].name;
        out_registry->crt_symbols[index].ordinal =
            g_windows_shim64_crt_exports[index].ordinal;
        out_registry->crt_symbols[index].address =
            context->windows_crt_base
                + (u64)g_windows_shim64_crt_exports[index].rva;
    }

    out_registry->libraries[0].dll_name = "ntdll.dll";
    out_registry->libraries[0].symbols = out_registry->ntdll_symbols;
    out_registry->libraries[0].symbol_count = WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT;
    out_registry->libraries[1].dll_name = "kernel32.dll";
    out_registry->libraries[1].symbols = out_registry->kernel32_symbols;
    out_registry->libraries[1].symbol_count = WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT;
    out_registry->libraries[2].dll_name = "msvcrt.dll";
    out_registry->libraries[2].symbols = out_registry->crt_symbols;
    out_registry->libraries[2].symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    out_registry->libraries[3].dll_name = "ucrtbase.dll";
    out_registry->libraries[3].symbols = out_registry->crt_symbols;
    out_registry->libraries[3].symbol_count = WINDOWS_SHIM64_CRT_SYMBOL_COUNT;
    out_registry->registry.libraries = out_registry->libraries;
    out_registry->registry.library_count = WINDOWS_SHIM64_FULL_LIBRARY_COUNT;
    g_windows_shim64_crt_last_error = WINDOWS_SHIM64_ERROR_NONE;
    return WINDOWS_SHIM64_OK;
}

const u8 *windows_shim64_ntdll_image(void)
{
    return g_windows_shim64_ntdll_image;
}

const u8 *windows_shim64_kernel32_image(void)
{
    if (g_windows_shim64_kernel32_image_ready == 0u)
    {
        windows_shim64_build_kernel32_image();
    }

    return g_windows_shim64_kernel32_image;
}

const u8 *windows_shim64_crt_image(void)
{
    if (g_windows_shim64_crt_image_ready == 0u)
    {
        windows_shim64_build_crt_image();
    }

    return g_windows_shim64_crt_image;
}

u32 windows_shim64_ntdll_image_size(void)
{
    return WINDOWS_SHIM64_NTDLL_FILE_BYTES;
}

u32 windows_shim64_kernel32_image_size(void)
{
    return WINDOWS_SHIM64_KERNEL32_FILE_BYTES;
}

u32 windows_shim64_crt_image_size(void)
{
    return WINDOWS_SHIM64_CRT_FILE_BYTES;
}

u32 windows_shim64_ntdll_load_count(void)
{
    return g_windows_shim64_ntdll_load_count;
}

u32 windows_shim64_ntdll_denial_count(void)
{
    return g_windows_shim64_ntdll_denial_count;
}

u32 windows_shim64_ntdll_last_error(void)
{
    return g_windows_shim64_ntdll_last_error;
}

u64 windows_shim64_ntdll_last_base(void)
{
    return g_windows_shim64_ntdll_last_base;
}

u32 windows_shim64_kernel32_load_count(void)
{
    return g_windows_shim64_kernel32_load_count;
}

u32 windows_shim64_kernel32_denial_count(void)
{
    return g_windows_shim64_kernel32_denial_count;
}

u32 windows_shim64_kernel32_last_error(void)
{
    return g_windows_shim64_kernel32_last_error;
}

u64 windows_shim64_kernel32_last_base(void)
{
    return g_windows_shim64_kernel32_last_base;
}

u32 windows_shim64_crt_load_count(void)
{
    return g_windows_shim64_crt_load_count;
}

u32 windows_shim64_crt_denial_count(void)
{
    return g_windows_shim64_crt_denial_count;
}

u32 windows_shim64_crt_last_error(void)
{
    return g_windows_shim64_crt_last_error;
}

u64 windows_shim64_crt_last_base(void)
{
    return g_windows_shim64_crt_last_base;
}
