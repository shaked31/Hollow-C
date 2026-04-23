#ifndef HOLLOWING_H
#define HOLLOWING_H

#include <windows.h>

typedef struct _PAYLOAD_INFO {
    PBYTE buffer; // Raw bytes of executable file
    DWORD size; // Size of the file in bytes
    PIMAGE_NT_HEADERS nt; // Pointer to the PE headers
    PBYTE entryPointRVA; // Relative virtual address where code begins
    ULONG_PTR imageBase; // Preferred base address of image (ULONG_PTR size depend on architecture)
} PAYLOAD_INFO, *PPAYLOAD_INFO;

BOOL ParsePayload(LPCSTR filePath, PPAYLOAD_INFO info);

#endif