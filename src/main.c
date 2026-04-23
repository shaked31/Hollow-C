#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;

    if (!CreateProcessA(
        "C:\\WINDOWS\\system32\\notepad.exe",
        NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
        NULL, NULL, &si, &pi)
    ) {
        printf("Error: Failed to create process: error %lu\n", GetLastError());
        return EXIT_FAILURE;
    }

    printf("Target process created suspended with PID: %lu\n", pi.dwProcessId);
    printf("Check now in task manager to see the process\n");
    getchar();

    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return EXIT_SUCCESS;
}