#include "hollowing.h"
#include <stdio.h>
#include <stdlib.h>


BOOL ParsePayload(LPCSTR filePath, PPAYLOAD_INFO info) {
    HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Couldn't open payload file (Error %lu)\n", GetLastError());
        return FALSE;
    }

    info->size = GetFileSize(hFile, NULL);
    info->buffer = (PBYTE)malloc(info->size);
    if (info->buffer == NULL) {
        fprintf(stderr, "Couldn't allocate memory to buffer (Error %lu)\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD bytesRead;
    ReadFile(hFile, info->buffer, info->size, &bytesRead, NULL);
    CloseHandle(hFile);

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)info->buffer;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return FALSE;
    }

    // e_lfanew is the offset that defines where the DOS header ends and the NT header begins.
    info->nt = (PIMAGE_NT_HEADERS)(info->buffer + dosHeader->e_lfanew);
    if (info->nt->Signature != IMAGE_NT_SIGNATURE) {
        return FALSE;
    }

    info->entryPointRVA = info->nt->OptionalHeader.AddressOfEntryPoint;
    info->imageBase = info->nt->OptionalHeader.ImageBase;
    info->imageSize = info->nt->OptionalHeader.SizeOfImage;

    printf("Payload %s parsed. EntryPoint RVA: 0x%lX\n", filePath, info->entryPointRVA);
    return TRUE;
}