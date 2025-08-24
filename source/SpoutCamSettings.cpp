//
// SpoutCamSettings.exe - Admin launcher for SpoutCam configuration
// This is a simple executable that launches the SpoutCam properties dialog
// with administrator privileges, equivalent to SpoutCamProperties.cmd
//

#include <windows.h>
#include <iostream>

typedef void (CALLBACK* ConfigureFunc)();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize COM
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize COM", L"SpoutCam Settings", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Determine which DLL to load based on platform
    const wchar_t* dllPath;
    #ifdef _WIN64
        dllPath = L"SpoutCam64\\SpoutCam64.ax";
    #else
        dllPath = L"SpoutCam32\\SpoutCam32.ax";
    #endif

    // Load the SpoutCam DLL
    HMODULE hDll = LoadLibrary(dllPath);
    if (!hDll) {
        MessageBox(nullptr, L"Failed to load SpoutCam DLL", L"SpoutCam Settings", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // Get the Configure function
    ConfigureFunc Configure = (ConfigureFunc)GetProcAddress(hDll, "Configure");
    if (!Configure) {
        MessageBox(nullptr, L"Failed to find Configure function", L"SpoutCam Settings", MB_OK | MB_ICONERROR);
        FreeLibrary(hDll);
        CoUninitialize();
        return 1;
    }

    // Call the Configure function
    Configure();

    // Cleanup
    FreeLibrary(hDll);
    CoUninitialize();
    
    return 0;
}

// Alternative main function for console builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}