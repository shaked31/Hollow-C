#include <windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    MessageBoxA(NULL, "hello from hollowed process!", "Shaked's Project", MB_OK);
    return 0;
}