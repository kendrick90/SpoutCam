//
// SpoutCamSettings.exe - Admin launcher for SpoutCam configuration
// This is a simple executable that launches the SpoutCam properties dialog
// with administrator privileges, equivalent to SpoutCamProperties.cmd
//

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Get the directory where this executable is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0; // Remove filename, keep directory

    // Determine which subdirectory and DLL to use based on platform
    const wchar_t* subDir;
    const wchar_t* dllName;
    #ifdef _WIN64
        subDir = L"SpoutCam64";
        dllName = L"SpoutCam64.ax";
    #else
        subDir = L"SpoutCam32"; 
        dllName = L"SpoutCam32.ax";
    #endif

    // Build path to DLL directory
    wchar_t dllDir[MAX_PATH];
    swprintf_s(dllDir, MAX_PATH, L"%s\\%s", exePath, subDir);

    // Build rundll32 parameters (exactly like the .cmd file does)
    wchar_t parameters[MAX_PATH];
    swprintf_s(parameters, MAX_PATH, L"%s,Configure", dllName);

    // Execute rundll32.exe with the DLL and Configure function, from the DLL directory
    // This mimics exactly what the .cmd file does: rundll32.exe SpoutCam64.ax,Configure
    HINSTANCE result = ShellExecute(
        nullptr,                    // parent window
        L"open",                    // operation
        L"rundll32.exe",           // program to run
        parameters,                // parameters: "SpoutCam64.ax,Configure"
        dllDir,                    // working directory (where the .ax file is)
        SW_SHOW                    // show window
    );

    if ((INT_PTR)result <= 32) {
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, 512, L"Failed to execute rundll32.exe\nDLL: %s\nDirectory: %s\nError code: %d", 
                  dllName, dllDir, (int)(INT_PTR)result);
        MessageBox(nullptr, errorMsg, L"SpoutCam Settings", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    return 0;
}

// Alternative main function for console builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}