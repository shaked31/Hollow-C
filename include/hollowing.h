#ifndef HOLLOWING_H
#define HOLLOWING_H

#include <windows.h>

typedef struct _PAYLOAD_INFO {
    PBYTE buffer; // Raw bytes of executable file
    DWORD size; // Size of the file in bytes
    DWORD imageSize; // Size when loaded in memory
    PIMAGE_NT_HEADERS nt; // Pointer to the PE headers
    DWORD entryPointRVA; // Relative virtual address where code begins (offset)
    ULONG_PTR imageBase; // Preferred base address of image (ULONG_PTR size depend on architecture)
} PAYLOAD_INFO, *PPAYLOAD_INFO;

BOOL ParsePayload(LPCSTR filePath, PPAYLOAD_INFO info);

#endif