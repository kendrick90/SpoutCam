//
// SpoutCamSettings.exe - Simple Camera Management Interface  
// No dialog resources needed - uses MessageBox interface with console logging
//

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "advapi32.lib")

// Forward declarations
bool IsRunningAsAdmin();
bool RestartAsAdmin();
void ShowManagementMenu();
void CleanupOrphanedCameras();
void ShowRegistrationTroubleshooting();
bool DeleteRegistrySettings(int cameraIndex);
bool IsCameraRegistered(int cameraIndex);
bool RegisterCamera(int cameraIndex);
bool UnregisterCamera(int cameraIndex);

// Console setup for debug output
void SetupConsole() 
{
    if (AllocConsole()) {
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        
        std::cout.clear();
        std::cerr.clear();
        std::cin.clear();
        
        SetConsoleTitle("SpoutCam Settings - Debug Console");
        printf("SpoutCam Settings Debug Console\n");
        printf("==============================\n\n");
    }
}

// Admin privilege checking functions
bool IsRunningAsAdmin() 
{
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool RestartAsAdmin()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    printf("Restarting with administrator privileges...\n");
    
    HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath, nullptr, nullptr, SW_SHOW);
    
    if ((INT_PTR)result > 32) {
        printf("Successfully launched elevated instance\n");
        return true;
    } else {
        printf("Failed to restart as admin (error %d)\n", (int)(INT_PTR)result);
        return false;
    }
}

// Registry utility function
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* pValue)
{
    printf("Reading registry: %s\\%s\n", subkey, valuename);
    
    HKEY key;
    if (RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        printf("  -> Key not found\n");
        return false;
    }
    
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    bool result = (RegQueryValueExA(key, valuename, nullptr, &type, (LPBYTE)pValue, &size) == ERROR_SUCCESS);
    
    if (result) {
        printf("  -> Found value: %lu\n", *pValue);
    } else {
        printf("  -> Value not found\n");
    }
    
    RegCloseKey(key);
    return result;
}

bool HasCameraSettings(int cameraIndex) 
{
    printf("\nChecking SpoutCam%d settings:\n", cameraIndex + 1);
    
    char keyName[256];
    DWORD testValue;
    
    // Build registry key name (match existing cam.cpp logic)
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    printf("Registry key: %s\n", keyName);
    
    // Check if any settings exist
    bool hasFps = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &testValue);
    bool hasRes = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &testValue);
    bool hasMirror = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &testValue);
    
    bool hasSettings = hasFps || hasRes || hasMirror;
    printf("SpoutCam%d status: %s\n", cameraIndex + 1, hasSettings ? "CONFIGURED" : "Default/Not Found");
    
    return hasSettings;
}

bool IsCameraRegistered(int cameraIndex)
{
    printf("Checking DirectShow registration for SpoutCam%d:\n", cameraIndex + 1);
    
    // Use PowerShell to check DirectShow filter registration
    // Based on build.bat logic for detecting registered SpoutCam filters
    char command[1024];
    sprintf_s(command, 
        "powershell -Command \""
        "$found = $false; "
        "Get-ChildItem 'HKLM:\\SOFTWARE\\Classes\\CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance' -ErrorAction SilentlyContinue | "
        "ForEach-Object { "
            "$friendlyName = Get-ItemProperty $_.PSPath -Name 'FriendlyName' -ErrorAction SilentlyContinue; "
            "if ($friendlyName.FriendlyName -match 'SpoutCam%d') { "
                "$found = $true "
            "} "
        "}; "
        "if ($found) { Write-Host 'REGISTERED' } else { Write-Host 'NOT_REGISTERED' }\""
        , cameraIndex == 0 ? 1 : cameraIndex + 1  // Match display naming
    );
    
    FILE* pipe = _popen(command, "r");
    if (!pipe) {
        printf("  -> Failed to check registration status\n");
        return false;
    }
    
    char result[256];
    bool registered = false;
    
    if (fgets(result, sizeof(result), pipe)) {
        registered = (strstr(result, "REGISTERED") != nullptr);
        printf("  -> Registration status: %s\n", registered ? "REGISTERED" : "NOT_REGISTERED");
    } else {
        printf("  -> No response from PowerShell command\n");
    }
    
    _pclose(pipe);
    return registered;
}

void OpenCameraProperties(int cameraIndex)
{
    // Get the directory where this executable is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture and build path to properties
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    
    wchar_t cmdPath[MAX_PATH];
    swprintf_s(cmdPath, MAX_PATH, L"%s\\%s\\SpoutCamProperties.cmd", exePath, subDir);
    
    // Open the camera properties
    ShellExecuteW(nullptr, L"runas", cmdPath, nullptr, nullptr, SW_SHOW);
}

bool RegisterCamera(int cameraIndex)
{
    printf("Attempting to register SpoutCam%d using regsvr32...\n", cameraIndex + 1);
    
    // Get the SpoutCam DLL path
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitWindows ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    printf("Using DLL: %ls\n", dllPath);
    
    // Use regsvr32 to register the DLL (this calls DllRegisterServer which registers camera 0)
    if (cameraIndex == 0) {
        wchar_t command[MAX_PATH + 50];
        swprintf_s(command, L"regsvr32.exe /s \"%s\"", dllPath);
        
        printf("Running: regsvr32 /s (silent register)\n");
        
        DWORD exitCode = 0;
        STARTUPINFOW si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        if (CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            if (exitCode == 0) {
                printf("Successfully registered SpoutCam%d\n", cameraIndex + 1);
                return true;
            } else {
                printf("regsvr32 failed with exit code: %lu\n", exitCode);
                
                // Try alternative method using the registration batch file
                printf("Trying alternative registration method...\n");
                wchar_t batchPath[MAX_PATH];
                swprintf_s(batchPath, MAX_PATH, L"%s\\%s\\regsvr_register.bat", exePath, subDir);
                
                HINSTANCE result = ShellExecuteW(nullptr, L"runas", batchPath, nullptr, nullptr, SW_HIDE);
                if ((INT_PTR)result > 32) {
                    printf("Alternative registration command launched\n");
                    // We can't easily check the result of ShellExecute batch file
                    // The user will need to check manually
                    return false; // Return false so we show troubleshooting
                } else {
                    printf("Alternative registration also failed\n");
                    return false;
                }
            }
        } else {
            printf("Failed to run regsvr32\n");
            return false;
        }
    } else {
        // For cameras other than SpoutCam1, we need individual registration
        // This would require a custom registration tool or different approach
        printf("Individual camera registration (camera %d) not supported via regsvr32\n", cameraIndex + 1);
        printf("Only SpoutCam1 can be registered this way\n");
        return false;
    }
}

bool UnregisterCamera(int cameraIndex)
{
    printf("Attempting to unregister all SpoutCam filters using regsvr32...\n");
    
    // Get the SpoutCam DLL path
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitWindows ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    printf("Using DLL: %ls\n", dllPath);
    
    // Use regsvr32 to unregister the DLL (this calls DllUnregisterServer which unregisters all cameras)
    wchar_t command[MAX_PATH + 50];
    swprintf_s(command, L"regsvr32.exe /u /s \"%s\"", dllPath);
    
    printf("Running: regsvr32 /u /s (silent unregister all)\n");
    
    DWORD exitCode = 0;
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exitCode == 0) {
            printf("Successfully unregistered all SpoutCam filters\n");
            return true;
        } else {
            printf("regsvr32 unregister failed with exit code: %lu\n", exitCode);
            return false;
        }
    } else {
        printf("Failed to run regsvr32\n");
        return false;
    }
}

void ShowCameraList()
{
    printf("\n=== SpoutCam Status Summary ===\n");
    
    char message[4096];
    char line[256];
    
    strcpy_s(message, "SpoutCam Camera Management\n");
    strcat_s(message, "==============================\n\n");
    
    bool hasRegisteredCameras = false;
    bool hasConfiguredCameras = false;
    
    for (int i = 0; i < 8; i++) {
        bool hasSettings = HasCameraSettings(i);
        bool isRegistered = IsCameraRegistered(i);
        
        if (hasSettings) hasConfiguredCameras = true;
        if (isRegistered) hasRegisteredCameras = true;
        
        sprintf_s(line, "SpoutCam%d: %s | %s\n", 
                 i + 1,
                 hasSettings ? "CONFIGURED" : "Default",
                 isRegistered ? "REGISTERED" : "Unregistered");
        strcat_s(message, line);
    }
    
    strcat_s(message, "\n");
    
    if (hasRegisteredCameras) {
        strcat_s(message, "REGISTERED cameras appear in video applications.\n");
    }
    
    if (hasConfiguredCameras && !hasRegisteredCameras) {
        strcat_s(message, "You have configured cameras that are NOT registered.\n");
    }
    
    strcat_s(message, "\nOptions:\n");
    strcat_s(message, "- OK: Manage cameras (register/unregister/configure)\n");
    strcat_s(message, "- Cancel: Exit\n");
    
    int result = MessageBox(nullptr, message, "SpoutCam Settings", MB_OKCANCEL | MB_ICONINFORMATION);
    
    if (result == IDOK) {
        ShowManagementMenu();
    }
}

void ShowManagementMenu()
{
    char message[1024];
    strcpy_s(message, "Camera Management Actions:\n\n");
    strcat_s(message, "1. Clean up orphaned cameras (unregister SpoutCam2-4)\n");
    strcat_s(message, "2. Register SpoutCam1 only\n");
    strcat_s(message, "3. Configure camera properties\n");
    strcat_s(message, "4. Refresh and show status again\n");
    strcat_s(message, "\nRecommendation: Start with option 1 to clean up,\n");
    strcat_s(message, "then option 2 to register just SpoutCam1.\n");
    strcat_s(message, "\nClick OK to clean up orphaned cameras, Cancel to exit.");
    
    int result = MessageBox(nullptr, message, "Camera Management", MB_OKCANCEL | MB_ICONQUESTION);
    
    if (result == IDOK) {
        CleanupOrphanedCameras();
    }
}

bool DeleteRegistrySettings(int cameraIndex)
{
    char keyName[256];
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    printf("Deleting registry settings: %s\n", keyName);
    
    HKEY parentKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Leading Edge", 0, KEY_WRITE, &parentKey) == ERROR_SUCCESS) {
        const char* subkeyName = (cameraIndex == 0) ? "SpoutCam" : (keyName + strlen("Software\\Leading Edge\\"));
        LONG result = RegDeleteKeyA(parentKey, subkeyName);
        RegCloseKey(parentKey);
        
        if (result == ERROR_SUCCESS) {
            printf("  -> Successfully deleted registry settings\n");
            return true;
        } else if (result == ERROR_FILE_NOT_FOUND) {
            printf("  -> Registry key already doesn't exist\n");
            return true;
        } else {
            printf("  -> Failed to delete registry key (error %ld)\n", result);
            return false;
        }
    } else {
        printf("  -> Failed to open parent registry key\n");
        return false;
    }
}

void CleanupOrphanedCameras()
{
    printf("\n=== Comprehensive cleanup of SpoutCam ===\n");
    
    // Step 1: Unregister all DirectShow filters
    printf("Step 1: Unregistering all DirectShow filters...\n");
    bool unregisterSuccess = UnregisterCamera(0);
    
    // Step 2: Clean up registry settings for cameras 2-4 (orphaned ones)
    printf("\nStep 2: Cleaning up orphaned registry settings...\n");
    bool registryCleanupSuccess = true;
    for (int i = 1; i < 4; i++) {
        bool hasSettings = HasCameraSettings(i);
        if (hasSettings) {
            printf("Cleaning up SpoutCam%d registry settings...\n", i + 1);
            if (!DeleteRegistrySettings(i)) {
                registryCleanupSuccess = false;
            }
        }
    }
    
    char message[1024];
    if (unregisterSuccess && registryCleanupSuccess) {
        strcpy_s(message, "Cleanup completed successfully!\n\n");
        strcat_s(message, "✓ All SpoutCam DirectShow filters unregistered\n");
        strcat_s(message, "✓ Orphaned registry settings cleaned up\n\n");
        strcat_s(message, "Would you like to register SpoutCam1 now?\n");
        strcat_s(message, "This will make only SpoutCam1 available in video applications.");
        
        int result = MessageBox(nullptr, message, "Cleanup Complete", MB_YESNO | MB_ICONQUESTION);
        
        if (result == IDYES) {
            printf("\nStep 3: Registering SpoutCam1...\n");
            if (RegisterCamera(0)) {
                MessageBox(nullptr, "Success! SpoutCam1 registered and ready to use.\n\nYou now have a clean setup:\n• Only SpoutCam1 is registered\n• No orphaned settings\n• Ready for video applications", "Setup Complete", MB_OK | MB_ICONINFORMATION);
            } else {
                ShowRegistrationTroubleshooting();
            }
        }
    } else {
        strcpy_s(message, "Cleanup had some issues:\n\n");
        if (!unregisterSuccess) {
            strcat_s(message, "✗ Failed to unregister DirectShow filters\n");
        } else {
            strcat_s(message, "✓ DirectShow filters unregistered\n");
        }
        if (!registryCleanupSuccess) {
            strcat_s(message, "✗ Some registry settings couldn't be cleaned\n");
        } else {
            strcat_s(message, "✓ Registry settings cleaned\n");
        }
        strcat_s(message, "\nCheck console for details.");
        
        MessageBox(nullptr, message, "Cleanup Issues", MB_OK | MB_ICONWARNING);
    }
}

void ShowRegistrationTroubleshooting()
{
    char message[1024];
    strcpy_s(message, "SpoutCam1 registration failed (Exit Code 5 = Access Denied)\n\n");
    strcat_s(message, "Possible solutions:\n");
    strcat_s(message, "1. Close all video applications (OBS, etc.)\n");
    strcat_s(message, "2. Make sure no SpoutCam filters are in use\n");
    strcat_s(message, "3. Try running as administrator again\n");
    strcat_s(message, "4. Restart Windows and try again\n\n");
    strcat_s(message, "You can also try the manual method:\n");
    strcat_s(message, "Use SpoutCamProperties.cmd for registration");
    
    MessageBox(nullptr, message, "Registration Help", MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetupConsole();
    printf("SpoutCamSettings starting...\n");
    
    // Check if running as administrator
    if (!IsRunningAsAdmin()) {
        printf("Requesting administrator privileges...\n");
        
        // Immediately restart with admin privileges - no dialog asking user
        if (RestartAsAdmin()) {
            return 0; // Exit this instance
        } else {
            printf("Failed to obtain administrator privileges\n");
            MessageBox(nullptr, "SpoutCam Settings requires administrator privileges but failed to restart.\n\nPlease run SpoutCamSettings as administrator manually.", "Admin Required", MB_OK | MB_ICONERROR);
            printf("Press any key to exit...\n");
            getchar();
            return 1;
        }
    }
    
    printf("Running with administrator privileges ✓\n\n");
    
    ShowCameraList();
    
    printf("\nPress any key to exit...\n");
    getchar();
    
    return 0;
}