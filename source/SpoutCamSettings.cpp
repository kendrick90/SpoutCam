//
// SpoutCamSettings.exe - Admin launcher for SpoutCam configuration
// This is a simple wrapper that runs the appropriate SpoutCamProperties.cmd file
//

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Get the directory where this executable is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0; // Remove filename, keep directory

    // Detect system architecture at runtime to choose the right .cmd file
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    
    const wchar_t* subDir;
    if (is64BitWindows) {
        // Check if 64-bit version exists
        wchar_t test64Path[MAX_PATH];
        swprintf_s(test64Path, MAX_PATH, L"%s\\SpoutCam64\\SpoutCamProperties.cmd", exePath);
        
        if (GetFileAttributesW(test64Path) != INVALID_FILE_ATTRIBUTES) {
            subDir = L"SpoutCam64";
        } else {
            subDir = L"SpoutCam32";  // Fall back to 32-bit
        }
    } else {
        subDir = L"SpoutCam32";  // 32-bit Windows
    }

    // Build path to the appropriate .cmd file
    wchar_t cmdPath[MAX_PATH];
    swprintf_s(cmdPath, MAX_PATH, L"%s\\%s\\SpoutCamProperties.cmd", exePath, subDir);

    // Execute the .cmd file with admin privileges - same pattern as both cmd files use
    HINSTANCE result = ShellExecuteW(
        nullptr,           // parent window
        L"runas",         // operation - request admin privileges
        cmdPath,          // path to SpoutCamProperties.cmd
        nullptr,          // no additional parameters
        nullptr,          // use default working directory
        SW_SHOW           // show window
    );

    if ((INT_PTR)result <= 32) {
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, 512, L"Failed to execute SpoutCamProperties.cmd\nPath: %s\nError code: %d", 
                  cmdPath, (int)(INT_PTR)result);
        MessageBoxW(nullptr, errorMsg, L"SpoutCam Settings", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    return 0;
}

// Alternative main function for console builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}