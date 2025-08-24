//
// SpoutCamSettings.exe - SpoutCam Management Interface  
// Individual camera registration and configuration
//

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include "resource.h"

// Utility function for basic logging
void LogWithTimestamp(const char* format, ...) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

#define LOG(...) LogWithTimestamp(__VA_ARGS__)

// Cache for registered filters for performance
std::vector<std::string> g_registeredFilters;
bool g_filtersScanned = false;

// Forward declarations
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void PopulateCameraList(HWND hListView);
bool RegisterCamera(int cameraIndex);
bool UnregisterCamera(int cameraIndex);
void OpenCameraProperties(int cameraIndex);
bool IsCameraRegistered(int cameraIndex);
bool HasCameraSettings(int cameraIndex);
void RefreshCameraList(HWND hListView);
bool IsRunningAsAdmin();
bool RestartAsAdmin();
void ScanRegisteredFilters();
bool ReadStringFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* buffer, DWORD bufferSize);
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* value);

// Function typedefs for DLL exports
typedef HRESULT (STDAPICALLTYPE *RegisterSingleSpoutCameraFunc)(int cameraIndex);
typedef HRESULT (STDAPICALLTYPE *UnregisterSingleSpoutCameraFunc)(int cameraIndex);

HINSTANCE g_hInst;

#define MAX_CAMERAS 8

// Optimized function to scan registered DirectShow video capture filters
void ScanRegisteredFilters()
{
    if (g_filtersScanned) {
        return; // Use cached results
    }
    
    g_registeredFilters.clear();
    
    // Direct registry access to CLSID_VideoInputDeviceCategory
    const char* directShowPath = "SOFTWARE\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance";
    HKEY instanceKey;
    
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, directShowPath, 0, KEY_READ, &instanceKey);
    if (result == ERROR_SUCCESS) {
        DWORD subkeyCount = 0;
        DWORD maxSubkeyLen = 0;
        result = RegQueryInfoKeyA(instanceKey, nullptr, nullptr, nullptr, &subkeyCount, &maxSubkeyLen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        
        if (result == ERROR_SUCCESS) {
            char* subkeyName = new char[maxSubkeyLen + 1];
            
            for (DWORD i = 0; i < subkeyCount; i++) {
                DWORD subkeyNameLen = maxSubkeyLen + 1;
                if (RegEnumKeyExA(instanceKey, i, subkeyName, &subkeyNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    HKEY filterKey;
                    if (RegOpenKeyExA(instanceKey, subkeyName, 0, KEY_READ, &filterKey) == ERROR_SUCCESS) {
                        char friendlyName[256];
                        if (ReadStringFromRegistry(HKEY_LOCAL_MACHINE, 
                                                 (std::string(directShowPath) + "\\" + subkeyName).c_str(), 
                                                 "FriendlyName", friendlyName, sizeof(friendlyName))) {
                            g_registeredFilters.push_back(std::string(friendlyName));
                        }
                        RegCloseKey(filterKey);
                    }
                }
            }
            delete[] subkeyName;
        }
        RegCloseKey(instanceKey);
    }
    
    g_filtersScanned = true;
}

// Setup console for debug output
void SetupConsole() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    std::cout.clear();
    std::cin.clear();
    std::cerr.clear();
}

bool ReadStringFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* buffer, DWORD bufferSize)
{
    HKEY key;
    LONG openResult = RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &key);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD type = REG_SZ;
    DWORD size = bufferSize;
    LONG queryResult = RegQueryValueExA(key, valuename, nullptr, &type, (LPBYTE)buffer, &size);
    bool result = (queryResult == ERROR_SUCCESS);
    
    RegCloseKey(key);
    return result;
}

bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* value)
{
    HKEY key;
    LONG openResult = RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &key);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    LONG queryResult = RegQueryValueExA(key, valuename, nullptr, &type, (LPBYTE)value, &size);
    bool result = (queryResult == ERROR_SUCCESS);
    
    RegCloseKey(key);
    return result;
}

bool IsCameraRegistered(int cameraIndex)
{
    // Use cached filter scan results with correct SpoutCam naming
    char searchName[32];
    if (cameraIndex == 0) {
        strcpy_s(searchName, "SpoutCam");  // First camera has no number
    } else {
        sprintf_s(searchName, "SpoutCam%d", cameraIndex + 1);  // SpoutCam2, SpoutCam3, etc.
    }
    
    for (const auto& filter : g_registeredFilters) {
        if (filter.find(searchName) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool HasCameraSettings(int cameraIndex) 
{
    char keyName[256];
    DWORD testValue;
    
    // Build registry key name (match existing cam.cpp logic)
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    // Check if any settings exist
    bool hasFps = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &testValue);
    bool hasResolution = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &testValue);
    bool hasMirror = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &testValue);
    
    return (hasFps || hasResolution || hasMirror);
}

void RefreshCameraList(HWND hListView)
{
    g_filtersScanned = false; // Force rescan
    ScanRegisteredFilters();
    PopulateCameraList(hListView);
}

void PopulateCameraList(HWND hListView)
{
    // Scan for registered filters first
    ScanRegisteredFilters();
    
    // Clear existing items
    ListView_DeleteAllItems(hListView);
    
    // Add each camera to the list
    for (int i = 0; i < MAX_CAMERAS; i++) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        
        char cameraName[32];
        sprintf_s(cameraName, "SpoutCam%d", i + 1);
        lvi.pszText = cameraName;
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        
        // Set registration status
        bool registered = IsCameraRegistered(i);
        ListView_SetItemText(hListView, itemIndex, 1, (LPSTR)(registered ? "Registered" : "Not Registered"));
        
        // Set configuration status
        bool hasSettings = HasCameraSettings(i);
        ListView_SetItemText(hListView, itemIndex, 2, (LPSTR)(hasSettings ? "Configured" : "Default"));
    }
}

bool RegisterCamera(int cameraIndex)
{
    // Detect process architecture
#ifdef _WIN64
    bool is64BitProcess = true;
#else
    bool is64BitProcess = false;
#endif
    
    const wchar_t* subDir = is64BitProcess ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitProcess ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    LOG("Loading SpoutCam DLL: %S\n", dllPath);
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        LOG("ERROR: Failed to load DLL, error code: %lu\n", GetLastError());
        return false;
    }
    
    RegisterSingleSpoutCameraFunc registerFunc = 
        (RegisterSingleSpoutCameraFunc)GetProcAddress(hDll, "RegisterSingleSpoutCamera");
    
    if (!registerFunc) {
        LOG("ERROR: Failed to find RegisterSingleSpoutCamera function\n");
        FreeLibrary(hDll);
        return false;
    }
    
    HRESULT hr = registerFunc(cameraIndex);
    LOG("Registration result: 0x%08X %s\n", hr, SUCCEEDED(hr) ? "(SUCCESS)" : "(FAILED)");
    
    FreeLibrary(hDll);
    return SUCCEEDED(hr);
}

bool UnregisterCamera(int cameraIndex)
{
    // Detect process architecture
#ifdef _WIN64
    bool is64BitProcess = true;
#else
    bool is64BitProcess = false;
#endif
    
    const wchar_t* subDir = is64BitProcess ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitProcess ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        LOG("ERROR: Failed to load DLL for unregistration\n");
        return false;
    }
    
    UnregisterSingleSpoutCameraFunc unregisterFunc = 
        (UnregisterSingleSpoutCameraFunc)GetProcAddress(hDll, "UnregisterSingleSpoutCamera");
    
    if (!unregisterFunc) {
        LOG("ERROR: Failed to find UnregisterSingleSpoutCamera function\n");
        FreeLibrary(hDll);
        return false;
    }
    
    HRESULT hr = unregisterFunc(cameraIndex);
    LOG("Unregistration result: 0x%08X %s\n", hr, SUCCEEDED(hr) ? "(SUCCESS)" : "(FAILED)");
    
    FreeLibrary(hDll);
    return SUCCEEDED(hr);
}

void OpenCameraProperties(int cameraIndex)
{
    // Detect process architecture
#ifdef _WIN64
    bool is64BitProcess = true;
#else
    bool is64BitProcess = false;
#endif
    
    const wchar_t* subDir = is64BitProcess ? L"SpoutCam64" : L"SpoutCam32";
    
    wchar_t cmdPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
    swprintf_s(cmdPath, MAX_PATH, L"%s\\%s\\SpoutCamProperties.cmd", exePath, subDir);
    
    wchar_t parameters[64];
    swprintf_s(parameters, 64, L"%d", cameraIndex);
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = cmdPath;
    sei.lpParameters = parameters;
    sei.nShow = SW_SHOW;
    
    if (!ShellExecuteExW(&sei)) {
        MessageBox(nullptr, "Failed to open camera properties", "Error", MB_OK | MB_ICONERROR);
    }
}

bool IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
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
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOW;
    
    return ShellExecuteExW(&sei) != FALSE;
}

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hListView = nullptr;
    
    switch (message) {
        case WM_INITDIALOG:
            {
                g_hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);
                
                // Get ListView handle
                hListView = GetDlgItem(hDlg, IDC_CAMERA_LIST);
                
                // Set up ListView columns
                LVCOLUMN lvc = {0};
                lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
                lvc.cx = 100;
                lvc.pszText = (LPSTR)"Camera";
                ListView_InsertColumn(hListView, 0, &lvc);
                
                lvc.cx = 120;
                lvc.pszText = (LPSTR)"Status";
                ListView_InsertColumn(hListView, 1, &lvc);
                
                lvc.cx = 100;
                lvc.pszText = (LPSTR)"Settings";
                ListView_InsertColumn(hListView, 2, &lvc);
                
                // Set ListView to report style and enable selection
                ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
                
                // Populate the camera list
                RefreshCameraList(hListView);
                
                return (INT_PTR)TRUE;
            }
            
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                
                switch (wmId) {
                    case IDC_REGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1) {
                                if (RegisterCamera(selected)) {
                                    g_filtersScanned = false; // Force rescan
                                    RefreshCameraList(hListView);
                                    LOG("Camera %d registered successfully\n", selected + 1);
                                } else {
                                    LOG("Failed to register camera %d\n", selected + 1);
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to register.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_UNREGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1) {
                                if (UnregisterCamera(selected)) {
                                    g_filtersScanned = false; // Force rescan
                                    RefreshCameraList(hListView);
                                    LOG("Camera %d unregistered successfully\n", selected + 1);
                                } else {
                                    LOG("Failed to unregister camera %d\n", selected + 1);
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to unregister.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_REFRESH:
                        g_filtersScanned = false;
                        RefreshCameraList(hListView);
                        break;
                        
                    case IDC_CLEANUP:
                        {
                            // Cleanup orphaned camera registrations for all 8 cameras
                            int result = MessageBox(hDlg, 
                                "This will unregister ALL SpoutCam cameras (1-8) to clean up orphaned registrations.\n\n"
                                "Are you sure you want to proceed?", 
                                "Cleanup All Cameras", 
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                
                            if (result == IDYES) {
                                LOG("Starting cleanup of all SpoutCam cameras...\n");
                                int successCount = 0;
                                int failCount = 0;
                                
                                // Unregister all 8 cameras
                                for (int i = 0; i < MAX_CAMERAS; i++) {
                                    LOG("Cleaning up SpoutCam%d...\n", i + 1);
                                    if (UnregisterCamera(i)) {
                                        successCount++;
                                        LOG("SpoutCam%d unregistered successfully\n", i + 1);
                                    } else {
                                        failCount++;
                                        LOG("Failed to unregister SpoutCam%d\n", i + 1);
                                    }
                                }
                                
                                // Force refresh to show updated status
                                g_filtersScanned = false;
                                RefreshCameraList(hListView);
                                
                                // Show summary
                                char summaryMsg[256];
                                sprintf_s(summaryMsg, 
                                    "Cleanup complete!\n\nSuccessfully unregistered: %d cameras\nFailed to unregister: %d cameras\n\n"
                                    "All SpoutCam registrations have been cleaned up.",
                                    successCount, failCount);
                                MessageBox(hDlg, summaryMsg, "Cleanup Complete", MB_OK | MB_ICONINFORMATION);
                                
                                LOG("Cleanup complete: %d success, %d failed\n", successCount, failCount);
                            } else {
                                LOG("Cleanup cancelled by user\n");
                            }
                        }
                        break;
                        
                    case IDC_CONFIGURE_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1) {
                                OpenCameraProperties(selected);
                            } else {
                                MessageBox(hDlg, "Please select a camera to configure.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDCANCEL:
                    case IDOK:
                        EndDialog(hDlg, LOWORD(wParam));
                        return (INT_PTR)TRUE;
                }
            }
            break;
    }
    
    return (INT_PTR)FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetupConsole();
    LOG("SpoutCam Settings - Camera Management Interface\n");
    
    // Check if running as administrator
    if (!IsRunningAsAdmin()) {
        LOG("Requesting administrator privileges...\n");
        if (RestartAsAdmin()) {
            return 0; // Exit this instance
        } else {
            MessageBox(nullptr, "SpoutCam Settings requires administrator privileges.", "Admin Required", MB_OK | MB_ICONERROR);
            return 1;
        }
    }
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icex)) {
        MessageBox(nullptr, "Failed to initialize common controls", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Launch the settings dialog
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_SETTINGS_MAIN), nullptr, SettingsDialogProc);
    
    return 0;
}