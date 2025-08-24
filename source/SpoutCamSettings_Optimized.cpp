//
// SpoutCamSettings.exe - Optimized Camera Management Interface
// Fast registry-based detection with PowerShell fallback
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

// Utility function for timestamped logging
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
#define LOG_FUNC_START(funcName) LOG(">>> ENTER: %s\n", funcName)
#define LOG_FUNC_END(funcName) LOG("<<< EXIT:  %s\n", funcName)
#define LOG_PERF_START(desc, var) DWORD var = GetTickCount(); LOG("PERF START: %s\n", desc)
#define LOG_PERF_END(desc, var) LOG("PERF END:   %s (took %lu ms)\n", desc, GetTickCount() - var)

// Cache for registered filters to avoid multiple PowerShell calls
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
void SetupConsole();
void ShowRegistrationTroubleshooting();
bool DeleteRegistrySettings(int cameraIndex);
void CleanupOrphanedCameras();
void ScanRegisteredFilters(); // New optimized function

// External DLL functions for individual camera registration
extern "C" {
    typedef HRESULT (STDAPICALLTYPE *RegisterSingleSpoutCameraFunc)(int cameraIndex);
    typedef HRESULT (STDAPICALLTYPE *UnregisterSingleSpoutCameraFunc)(int cameraIndex);
}

// Include required libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

// Enhanced Registry utility function with debug output  
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* pValue)
{
    const char* hkeyName = (hKey == HKEY_CURRENT_USER) ? "HKEY_CURRENT_USER" : 
                          (hKey == HKEY_LOCAL_MACHINE) ? "HKEY_LOCAL_MACHINE" : "UNKNOWN";
    LOG("    Reading: %s\\%s\\%s\n", hkeyName, subkey, valuename);
    
    HKEY key;
    LONG openResult = RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &key);
    if (openResult != ERROR_SUCCESS) {
        LOG("    -> Key not found (error %ld)\n", openResult);
        return false;
    }
    
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    LONG queryResult = RegQueryValueExA(key, valuename, nullptr, &type, (LPBYTE)pValue, &size);
    bool result = (queryResult == ERROR_SUCCESS);
    
    if (result) {
        LOG("    -> Found value: %lu\n", *pValue);
    } else {
        LOG("    -> Value not found (error %ld)\n", queryResult);
    }
    
    RegCloseKey(key);
    return result;
}

// Enhanced registry string reading
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

#define MAX_CAMERAS 8

// Optimized function to scan all registered DirectShow video capture filters once
void ScanRegisteredFilters()
{
    LOG_FUNC_START("ScanRegisteredFilters");
    LOG_PERF_START("Filter scan", scanPerf);
    
    if (g_filtersScanned) {
        LOG("Filters already scanned, using cache\n");
        LOG_FUNC_END("ScanRegisteredFilters");
        return;
    }
    
    g_registeredFilters.clear();
    
    LOG("Method 1: Direct registry access to DirectShow filters...\n");
    
    // Try direct registry access first (much faster)
    const char* directShowPath = "SOFTWARE\\Classes\\CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance";
    HKEY instanceKey;
    
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, directShowPath, 0, KEY_READ, &instanceKey);
    if (result == ERROR_SUCCESS) {
        LOG("Successfully opened DirectShow Instance registry key\n");
        
        DWORD subkeyCount = 0;
        DWORD maxSubkeyLen = 0;
        result = RegQueryInfoKeyA(instanceKey, nullptr, nullptr, nullptr, &subkeyCount, &maxSubkeyLen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        
        if (result == ERROR_SUCCESS) {
            LOG("Found %lu DirectShow filter instances\n", subkeyCount);
            
            char* subkeyName = new char[maxSubkeyLen + 1];
            
            for (DWORD i = 0; i < subkeyCount; i++) {
                DWORD subkeyNameLen = maxSubkeyLen + 1;
                if (RegEnumKeyExA(instanceKey, i, subkeyName, &subkeyNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    
                    // Open this specific filter's key
                    HKEY filterKey;
                    if (RegOpenKeyExA(instanceKey, subkeyName, 0, KEY_READ, &filterKey) == ERROR_SUCCESS) {
                        char friendlyName[256];
                        if (ReadStringFromRegistry(HKEY_LOCAL_MACHINE, 
                                                 (std::string(directShowPath) + "\\" + subkeyName).c_str(), 
                                                 "FriendlyName", friendlyName, sizeof(friendlyName))) {
                            LOG("Found filter: %s\n", friendlyName);
                            g_registeredFilters.push_back(std::string(friendlyName));
                        }
                        RegCloseKey(filterKey);
                    }
                }
            }
            
            delete[] subkeyName;
        }
        RegCloseKey(instanceKey);
    } else {
        LOG("Failed to open DirectShow registry key (error %ld)\n", result);
    }
    
    // If direct registry failed or found no SpoutCam filters, try PowerShell fallback
    bool foundSpoutCam = false;
    for (const auto& filter : g_registeredFilters) {
        if (filter.find("SpoutCam") != std::string::npos) {
            foundSpoutCam = true;
            break;
        }
    }
    
    if (!foundSpoutCam && g_registeredFilters.empty()) {
        LOG("Method 2: PowerShell fallback (slow but comprehensive)...\n");
        
        // Enhanced PowerShell command with better error handling
        const char* psCommand = "powershell -ExecutionPolicy Bypass -Command \""
            "try { "
                "$ErrorActionPreference = 'Stop'; "
                "$filters = @(); "
                "$regPath = 'HKLM:\\SOFTWARE\\Classes\\CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance'; "
                "Write-Host 'PS_DEBUG: Checking path:' $regPath; "
                "if (Test-Path $regPath) { "
                    "Write-Host 'PS_DEBUG: Path exists, scanning...'; "
                    "Get-ChildItem $regPath -ErrorAction SilentlyContinue | ForEach-Object { "
                        "$friendlyName = Get-ItemProperty $_.PSPath -Name 'FriendlyName' -ErrorAction SilentlyContinue; "
                        "if ($friendlyName.FriendlyName) { "
                            "$filters += $friendlyName.FriendlyName; "
                            "Write-Host 'PS_DEBUG: Found:' $friendlyName.FriendlyName; "
                        "} "
                    "}; "
                "} else { "
                    "Write-Host 'PS_DEBUG: Registry path does not exist'; "
                "} "
                "Write-Host 'ALL_FILTERS:' ($filters -join '|'); "
                "Write-Host 'PS_DEBUG: Scan complete, found' $filters.Count 'filters'; "
            "} catch { "
                "Write-Host 'PS_ERROR:' $_.Exception.Message; "
            "}\"";
        
        LOG("PowerShell Command: %s\n", psCommand);
        
        LOG_PERF_START("PowerShell execution", psPerf);
        FILE* pipe = _popen(psCommand, "r");
        if (pipe) {
            char result[4096];
            LOG("PowerShell Output:\n");
            while (fgets(result, sizeof(result), pipe)) {
                LOG("PS> %s", result);
                
                // Parse ALL_FILTERS line
                if (strstr(result, "ALL_FILTERS:") != nullptr) {
                    char* filtersStart = strstr(result, "ALL_FILTERS:") + 12;
                    if (strlen(filtersStart) > 1) {
                        // Parse pipe-separated filter names
                        char* token = strtok(filtersStart, "|");
                        while (token != nullptr) {
                            // Trim whitespace
                            while (*token == ' ') token++;
                            char* end = token + strlen(token) - 1;
                            while (end > token && (*end == ' ' || *end == '\n' || *end == '\r')) end--;
                            *(end + 1) = '\0';
                            
                            if (strlen(token) > 0) {
                                g_registeredFilters.push_back(std::string(token));
                                LOG("Parsed filter from PS: %s\n", token);
                            }
                            token = strtok(nullptr, "|");
                        }
                    }
                }
            }
            _pclose(pipe);
        } else {
            LOG("ERROR: Failed to execute PowerShell command\n");
        }
        LOG_PERF_END("PowerShell execution", psPerf);
    }
    
    LOG("=== FINAL FILTER SCAN RESULTS ===\n");
    LOG("Total registered video capture filters: %zu\n", g_registeredFilters.size());
    for (size_t i = 0; i < g_registeredFilters.size(); i++) {
        LOG("  [%zu] %s\n", i, g_registeredFilters[i].c_str());
    }
    LOG("=== END FILTER SCAN RESULTS ===\n");
    
    g_filtersScanned = true;
    LOG_PERF_END("Filter scan", scanPerf);
    LOG_FUNC_END("ScanRegisteredFilters");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetupConsole();
    LOG_FUNC_START("WinMain");
    
    // Check if running as administrator
    if (!IsRunningAsAdmin()) {
        LOG("Requesting administrator privileges...\n");
        
        // Automatically restart with admin privileges
        if (RestartAsAdmin()) {
            return 0; // Exit this instance
        } else {
            LOG("Failed to obtain administrator privileges\n");
            MessageBox(nullptr, "SpoutCam Settings requires administrator privileges but failed to restart.\n\nPlease run SpoutCamSettings as administrator manually.", "Admin Required", MB_OK | MB_ICONERROR);
            return 1;
        }
    }
    
    LOG("Running with administrator privileges [OK]\n\n");
    
    // Initialize common controls
    LOG("Initializing common controls...\n");
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icex)) {
        LOG("ERROR: Failed to initialize common controls\n");
        MessageBox(nullptr, "Failed to initialize common controls", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    LOG("Common controls initialized successfully\n");

    // Scan all registered filters once at startup
    LOG("Performing one-time filter scan...\n");
    ScanRegisteredFilters();

    // Create main settings dialog
    LOG("Creating main settings dialog...\n");
    INT_PTR result = DialogBox(hInstance, MAKEINTRESOURCE(IDD_SETTINGS_MAIN), nullptr, SettingsDialogProc);
    
    if (result == -1) {
        char errorMsg[256];
        DWORD error = GetLastError();
        sprintf_s(errorMsg, "DialogBox failed with error: %lu", error);
        LOG("ERROR: %s\n", errorMsg);
        MessageBox(nullptr, errorMsg, "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    LOG_FUNC_END("WinMain");
    return 0;
}

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hListView = nullptr;
    
    switch (message)
    {
    case WM_INITDIALOG:
        {
            LOG_FUNC_START("SettingsDialogProc - WM_INITDIALOG");
            
            // Set dialog title
            SetWindowText(hDlg, "SpoutCam Settings - Optimized");
            LOG("Dialog title set\n");
            
            // Center dialog on screen
            RECT rect;
            GetWindowRect(hDlg, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
            SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
            LOG("Dialog centered on screen\n");
            
            // Initialize list view
            hListView = GetDlgItem(hDlg, IDC_CAMERA_LIST);
            if (hListView) {
                LOG("Setting up ListView columns...\n");
                // Set up list view columns
                LVCOLUMN lvc = {0};
                lvc.mask = LVCF_TEXT | LVCF_WIDTH;
                
                lvc.pszText = "Camera";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 0, &lvc);
                
                lvc.pszText = "Status";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 1, &lvc);
                
                lvc.pszText = "Settings";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 2, &lvc);
                
                // Set full row select
                ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
                LOG("ListView columns configured\n");
                
                LOG("Starting initial camera list population...\n");
                PopulateCameraList(hListView);
            } else {
                LOG("ERROR: Failed to get ListView handle\n");
            }
            
            LOG_FUNC_END("SettingsDialogProc - WM_INITDIALOG");
            return TRUE;
        }
        
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            LOG("WM_COMMAND received: ID=%d\n", wmId);
            
            switch (wmId)
            {
            case IDC_CONFIGURE_CAMERA:
                {
                    LOG_FUNC_START("Configure Camera Button");
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    LOG("Selected camera index: %d\n", selected);
                    if (selected != -1) {
                        OpenCameraProperties(selected);
                    } else {
                        LOG("No camera selected for configuration\n");
                        MessageBox(hDlg, "Please select a camera to configure.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                    LOG_FUNC_END("Configure Camera Button");
                }
                break;
                
            case IDC_REGISTER_CAMERA:
                {
                    LOG_FUNC_START("Register Camera Button");
                    LOG_PERF_START("Full registration process", regPerf);
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    LOG("Selected camera index: %d\n", selected);
                    if (selected != -1) {
                        LOG("\n*** STARTING REGISTRATION PROCESS ***\n");
                        if (RegisterCamera(selected)) {
                            LOG("Registration reported success - invalidating filter cache...\n");
                            g_filtersScanned = false; // Force rescan
                            LOG("Waiting 1 second for system to update...\n");
                            Sleep(1000);
                            RefreshCameraList(hListView);
                        } else {
                            LOG("Registration reported failure\n");
                        }
                        LOG("*** REGISTRATION PROCESS COMPLETE ***\n\n");
                    } else {
                        LOG("No camera selected for registration\n");
                        MessageBox(hDlg, "Please select a camera to register.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                    LOG_PERF_END("Full registration process", regPerf);
                    LOG_FUNC_END("Register Camera Button");
                }
                break;
                
            case IDC_UNREGISTER_CAMERA:
                {
                    LOG_FUNC_START("Unregister Camera Button");
                    LOG_PERF_START("Full unregistration process", unregPerf);
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    LOG("Selected camera index: %d\n", selected);
                    if (selected != -1) {
                        // Confirm unregistration
                        char confirmMsg[256];
                        sprintf_s(confirmMsg, "Are you sure you want to unregister SpoutCam%d?\n\nThis will make it unavailable in video applications.", selected + 1);
                        int result = MessageBox(hDlg, confirmMsg, "Confirm Unregistration", MB_YESNO | MB_ICONQUESTION);
                        if (result == IDYES) {
                            LOG("\n*** STARTING UNREGISTRATION PROCESS ***\n");
                            if (UnregisterCamera(selected)) {
                                LOG("Unregistration reported success - invalidating filter cache...\n");
                                g_filtersScanned = false; // Force rescan
                                LOG("Waiting 1 second for system to update...\n");
                                Sleep(1000);
                                RefreshCameraList(hListView);
                            } else {
                                LOG("Unregistration reported failure\n");
                            }
                            LOG("*** UNREGISTRATION PROCESS COMPLETE ***\n\n");
                        } else {
                            LOG("User cancelled unregistration\n");
                        }
                    } else {
                        LOG("No camera selected for unregistration\n");
                        MessageBox(hDlg, "Please select a camera to unregister.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                    LOG_PERF_END("Full unregistration process", unregPerf);
                    LOG_FUNC_END("Unregister Camera Button");
                }
                break;
                
            case IDC_REFRESH:
                LOG_FUNC_START("Refresh Button");
                LOG("Manual refresh requested - invalidating filter cache\n");
                g_filtersScanned = false; // Force rescan
                RefreshCameraList(hListView);
                LOG_FUNC_END("Refresh Button");
                break;
                
            case IDC_CLEANUP:
                {
                    LOG_FUNC_START("Cleanup Button");
                    int result = MessageBox(hDlg, "This will clean up orphaned registry settings for cameras 2-8.\n\nDo you want to continue?", "Cleanup Confirmation", MB_YESNO | MB_ICONQUESTION);
                    if (result == IDYES) {
                        LOG("User confirmed cleanup\n");
                        CleanupOrphanedCameras();
                        g_filtersScanned = false; // Force rescan after cleanup
                        RefreshCameraList(hListView);
                    } else {
                        LOG("User cancelled cleanup\n");
                    }
                    LOG_FUNC_END("Cleanup Button");
                }
                break;
                
            case IDCANCEL:
            case IDOK:
                LOG("Dialog closing with result: %d\n", LOWORD(wParam));
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
        }
        break;
        
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == IDC_CAMERA_LIST && pnmh->code == NM_DBLCLK) {
                LOG("Double-click detected on camera list\n");
                // Double-click to configure
                int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                if (selected != -1) {
                    LOG("Double-click opening properties for camera %d\n", selected);
                    OpenCameraProperties(selected);
                }
            }
        }
        break;
    }
    return FALSE;
}

void PopulateCameraList(HWND hListView)
{
    LOG_FUNC_START("PopulateCameraList");
    LOG_PERF_START("Full camera list refresh", totalPerf);
    LOG("\n=====================================\n");
    LOG("REFRESHING CAMERA LIST (OPTIMIZED)\n");
    LOG("=====================================\n");
    
    // Ensure filters are scanned
    ScanRegisteredFilters();
    
    ListView_DeleteAllItems(hListView);
    LOG("Cleared existing ListView items\n");
    
    for (int i = 0; i < MAX_CAMERAS; i++) {
        LOG_PERF_START("Single camera processing", cameraPerf);
        LOG("\nProcessing camera %d of %d...\n", i + 1, MAX_CAMERAS);
        
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        
        // Camera name
        char cameraName[32];
        sprintf_s(cameraName, "SpoutCam%d", i + 1);
        lvi.pszText = cameraName;
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        LOG("Added camera to ListView at index %d\n", itemIndex);
        
        // Status column - use cached results
        bool registered = IsCameraRegistered(i);
        ListView_SetItemText(hListView, itemIndex, 1, registered ? "Registered" : "Not Registered");
        LOG("Set registration status: %s\n", registered ? "Registered" : "Not Registered");
        
        // Settings column  
        bool hasSettings = HasCameraSettings(i);
        ListView_SetItemText(hListView, itemIndex, 2, hasSettings ? "Configured" : "Default");
        LOG("Set settings status: %s\n", hasSettings ? "Configured" : "Default");
        
        LOG_PERF_END("Single camera processing", cameraPerf);
    }
    
    LOG("\n=====================================\n");
    LOG("CAMERA LIST REFRESH COMPLETE\n");
    LOG("=====================================\n\n");
    
    LOG_PERF_END("Full camera list refresh", totalPerf);
    LOG_FUNC_END("PopulateCameraList");
}

void RefreshCameraList(HWND hListView)
{
    LOG_FUNC_START("RefreshCameraList");
    PopulateCameraList(hListView);
    LOG_FUNC_END("RefreshCameraList");
}

bool IsCameraRegistered(int cameraIndex)
{
    LOG_FUNC_START("IsCameraRegistered");
    LOG_PERF_START("Registration check (cached)", regCheckPerf);
    LOG("Checking registration for SpoutCam%d using cached results\n", cameraIndex + 1);
    
    // Use cached filter scan results
    char searchName[32];
    sprintf_s(searchName, "SpoutCam%d", cameraIndex + 1);
    
    bool registered = false;
    for (const auto& filter : g_registeredFilters) {
        if (filter.find(searchName) != std::string::npos) {
            registered = true;
            LOG("Found matching filter: %s\n", filter.c_str());
            break;
        }
    }
    
    LOG("SpoutCam%d registration status: %s\n", cameraIndex + 1, registered ? "REGISTERED" : "NOT_REGISTERED");
    
    LOG_PERF_END("Registration check (cached)", regCheckPerf);
    LOG_FUNC_END("IsCameraRegistered");
    return registered;
}

bool HasCameraSettings(int cameraIndex) 
{
    LOG_FUNC_START("HasCameraSettings");
    LOG_PERF_START("Settings check", settingsPerf);
    LOG("Checking settings for SpoutCam%d\n", cameraIndex + 1);
    
    char keyName[256];
    DWORD testValue;
    
    // Build registry key name (match existing cam.cpp logic)
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    LOG("Checking registry key: HKEY_CURRENT_USER\\%s\n", keyName);
    
    // Check if any settings exist
    bool hasFps = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &testValue);
    bool hasRes = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &testValue);
    bool hasMirror = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &testValue);
    
    bool hasSettings = hasFps || hasRes || hasMirror;
    LOG("Settings: fps=%s, resolution=%s, mirror=%s -> Overall: %s\n", 
           hasFps ? "YES" : "NO", hasRes ? "YES" : "NO", hasMirror ? "YES" : "NO",
           hasSettings ? "CONFIGURED" : "DEFAULT");
    
    LOG_PERF_END("Settings check", settingsPerf);
    LOG_FUNC_END("HasCameraSettings");
    return hasSettings;
}

void OpenCameraProperties(int cameraIndex)
{
    LOG_FUNC_START("OpenCameraProperties");
    LOG("Opening properties for SpoutCam%d\n", cameraIndex + 1);
    
    // Get the directory where this executable is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture and build path to properties
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    LOG("Architecture detected: %s\n", is64BitWindows ? "x64" : "x86");
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    
    wchar_t cmdPath[MAX_PATH];
    swprintf_s(cmdPath, MAX_PATH, L"%s\\%s\\SpoutCamProperties.cmd", exePath, subDir);
    
    // Pass camera index as parameter to properties dialog
    wchar_t params[32];
    swprintf_s(params, L"%d", cameraIndex);
    LOG("Properties command path: %ls\n", cmdPath);
    LOG("Parameters: %ls\n", params);
    
    // Check if camera is registered before opening properties
    if (!IsCameraRegistered(cameraIndex)) {
        LOG("Camera is not registered - showing warning\n");
        char warningMsg[512];
        sprintf_s(warningMsg, 
            "SpoutCam%d is not currently registered.\n\n"
            "You can still configure its settings, but the camera won't appear in video applications until it's registered.\n\n"
            "Do you want to open the properties anyway?", 
            cameraIndex + 1);
        
        int result = MessageBox(nullptr, warningMsg, "Camera Not Registered", MB_YESNO | MB_ICONQUESTION);
        if (result != IDYES) {
            LOG("User cancelled properties dialog\n");
            LOG_FUNC_END("OpenCameraProperties");
            return;
        }
    }
    
    // Open the camera properties with camera index parameter
    LOG("Launching properties dialog with admin privileges...\n");
    ShellExecuteW(nullptr, L"runas", cmdPath, params, nullptr, SW_SHOW);
    LOG_FUNC_END("OpenCameraProperties");
}

bool RegisterCamera(int cameraIndex)
{
    LOG_FUNC_START("RegisterCamera");
    LOG_PERF_START("Camera registration", regCameraPerf);
    LOG("Attempting to register SpoutCam%d\n", cameraIndex + 1);
    
    // Get path to SpoutCam DLL
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    LOG("Architecture: %s\n", is64BitWindows ? "x64" : "x86");
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitWindows ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    LOG("DLL Path: %ls\n", dllPath);
    
    // Load the DLL and get the registration function
    LOG("Loading SpoutCam DLL...\n");
    LOG_PERF_START("DLL load", dllLoadPerf);
    HMODULE hDll = LoadLibraryW(dllPath);
    LOG_PERF_END("DLL load", dllLoadPerf);
    
    if (!hDll) {
        DWORD error = GetLastError();
        LOG("ERROR: Failed to load DLL, error code: %lu\n", error);
        char errorMsg[512];
        sprintf_s(errorMsg, "Failed to load SpoutCam DLL:\n%ls\n\nError: %lu", dllPath, error);
        MessageBox(nullptr, errorMsg, "Registration Error", MB_OK | MB_ICONERROR);
        LOG_FUNC_END("RegisterCamera");
        return false;
    }
    LOG("DLL loaded successfully\n");
    
    RegisterSingleSpoutCameraFunc registerFunc = 
        (RegisterSingleSpoutCameraFunc)GetProcAddress(hDll, "RegisterSingleSpoutCamera");
    
    if (!registerFunc) {
        LOG("ERROR: Failed to find RegisterSingleSpoutCamera function\n");
        FreeLibrary(hDll);
        MessageBox(nullptr, "Failed to find RegisterSingleSpoutCamera function in DLL", "Registration Error", MB_OK | MB_ICONERROR);
        LOG_FUNC_END("RegisterCamera");
        return false;
    }
    LOG("Registration function found\n");
    
    LOG("Calling RegisterSingleSpoutCamera(%d)...\n", cameraIndex);
    LOG_PERF_START("Registration call", regCallPerf);
    HRESULT hr = registerFunc(cameraIndex);
    LOG_PERF_END("Registration call", regCallPerf);
    LOG("Registration function returned: 0x%08X (%s)\n", hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
    
    FreeLibrary(hDll);
    LOG("DLL unloaded\n");
    
    if (SUCCEEDED(hr)) {
        char successMsg[256];
        sprintf_s(successMsg, "Successfully registered SpoutCam%d\n\nThe camera is now available in video applications.", cameraIndex + 1);
        MessageBox(nullptr, successMsg, "Registration Success", MB_OK | MB_ICONINFORMATION);
        LOG_PERF_END("Camera registration", regCameraPerf);
        LOG_FUNC_END("RegisterCamera");
        return true;
    } else {
        char errorMsg[256];
        sprintf_s(errorMsg, "Failed to register SpoutCam%d\n\nError code: 0x%08X", cameraIndex + 1, hr);
        MessageBox(nullptr, errorMsg, "Registration Failed", MB_OK | MB_ICONERROR);
        
        // Show troubleshooting help for common registration failures
        if (hr == 0x80070005) { // Access denied
            LOG("Showing troubleshooting for access denied error\n");
            ShowRegistrationTroubleshooting();
        }
        LOG_PERF_END("Camera registration", regCameraPerf);
        LOG_FUNC_END("RegisterCamera");
        return false;
    }
}

bool UnregisterCamera(int cameraIndex)
{
    LOG_FUNC_START("UnregisterCamera");
    LOG_PERF_START("Camera unregistration", unregCameraPerf);
    LOG("Attempting to unregister SpoutCam%d\n", cameraIndex + 1);
    
    // Get path to SpoutCam DLL
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    LOG("Architecture: %s\n", is64BitWindows ? "x64" : "x86");
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    const wchar_t* dllName = is64BitWindows ? L"SpoutCam64.ax" : L"SpoutCam32.ax";
    
    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    LOG("DLL Path: %ls\n", dllPath);
    
    // Load the DLL and get the unregistration function
    LOG("Loading SpoutCam DLL...\n");
    LOG_PERF_START("DLL load", dllLoadPerf);
    HMODULE hDll = LoadLibraryW(dllPath);
    LOG_PERF_END("DLL load", dllLoadPerf);
    
    if (!hDll) {
        DWORD error = GetLastError();
        LOG("ERROR: Failed to load DLL, error code: %lu\n", error);
        char errorMsg[512];
        sprintf_s(errorMsg, "Failed to load SpoutCam DLL:\n%ls\n\nError: %lu", dllPath, error);
        MessageBox(nullptr, errorMsg, "Unregistration Error", MB_OK | MB_ICONERROR);
        LOG_FUNC_END("UnregisterCamera");
        return false;
    }
    LOG("DLL loaded successfully\n");
    
    UnregisterSingleSpoutCameraFunc unregisterFunc = 
        (UnregisterSingleSpoutCameraFunc)GetProcAddress(hDll, "UnregisterSingleSpoutCamera");
    
    if (!unregisterFunc) {
        LOG("ERROR: Failed to find UnregisterSingleSpoutCamera function\n");
        FreeLibrary(hDll);
        MessageBox(nullptr, "Failed to find UnregisterSingleSpoutCamera function in DLL", "Unregistration Error", MB_OK | MB_ICONERROR);
        LOG_FUNC_END("UnregisterCamera");
        return false;
    }
    LOG("Unregistration function found\n");
    
    LOG("Calling UnregisterSingleSpoutCamera(%d)...\n", cameraIndex);
    LOG_PERF_START("Unregistration call", unregCallPerf);
    HRESULT hr = unregisterFunc(cameraIndex);
    LOG_PERF_END("Unregistration call", unregCallPerf);
    LOG("Unregistration function returned: 0x%08X (%s)\n", hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
    
    FreeLibrary(hDll);
    LOG("DLL unloaded\n");
    
    if (SUCCEEDED(hr)) {
        char successMsg[256];
        sprintf_s(successMsg, "Successfully unregistered SpoutCam%d\n\nThe camera is no longer available in video applications.", cameraIndex + 1);
        MessageBox(nullptr, successMsg, "Unregistration Success", MB_OK | MB_ICONINFORMATION);
        LOG_PERF_END("Camera unregistration", unregCameraPerf);
        LOG_FUNC_END("UnregisterCamera");
        return true;
    } else {
        char errorMsg[256];
        sprintf_s(errorMsg, "Failed to unregister SpoutCam%d\n\nError code: 0x%08X", cameraIndex + 1, hr);
        MessageBox(nullptr, errorMsg, "Unregistration Failed", MB_OK | MB_ICONERROR);
        
        // Show troubleshooting help for common unregistration failures
        if (hr == 0x80070005) { // Access denied
            LOG("Showing troubleshooting for access denied error\n");
            ShowRegistrationTroubleshooting();
        }
        LOG_PERF_END("Camera unregistration", unregCameraPerf);
        LOG_FUNC_END("UnregisterCamera");
        return false;
    }
}

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
        
        SetConsoleTitle("SpoutCam Settings - Optimized Debug Console");
        LOG("SpoutCam Settings Optimized Debug Console\n");
        LOG("==========================================\n\n");
    }
}

// Admin privilege checking functions
bool IsRunningAsAdmin() 
{
    LOG_FUNC_START("IsRunningAsAdmin");
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    bool result = (isAdmin == TRUE);
    LOG("Admin check result: %s\n", result ? "IS_ADMIN" : "NOT_ADMIN");
    LOG_FUNC_END("IsRunningAsAdmin");
    return result;
}

bool RestartAsAdmin()
{
    LOG_FUNC_START("RestartAsAdmin");
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    LOG("Restarting with administrator privileges...\n");
    LOG("Executable path: %ls\n", exePath);
    
    HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath, nullptr, nullptr, SW_SHOW);
    
    if ((INT_PTR)result > 32) {
        LOG("Successfully launched elevated instance\n");
        LOG_FUNC_END("RestartAsAdmin");
        return true;
    } else {
        LOG("Failed to restart as admin (error %d)\n", (int)(INT_PTR)result);
        LOG_FUNC_END("RestartAsAdmin");
        return false;
    }
}

void ShowRegistrationTroubleshooting()
{
    LOG_FUNC_START("ShowRegistrationTroubleshooting");
    char message[1024];
    strcpy_s(message, "Camera registration failed\n\n");
    strcat_s(message, "Possible solutions:\n");
    strcat_s(message, "1. Close all video applications (OBS, etc.)\n");
    strcat_s(message, "2. Make sure no SpoutCam filters are in use\n");
    strcat_s(message, "3. Try running as administrator again\n");
    strcat_s(message, "4. Restart Windows and try again\n\n");
    strcat_s(message, "You can also try the manual method:\n");
    strcat_s(message, "Use SpoutCamProperties.cmd for registration");
    
    MessageBox(nullptr, message, "Registration Help", MB_OK | MB_ICONINFORMATION);
    LOG_FUNC_END("ShowRegistrationTroubleshooting");
}

bool DeleteRegistrySettings(int cameraIndex)
{
    LOG_FUNC_START("DeleteRegistrySettings");
    char keyName[256];
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    LOG("Deleting registry settings: %s\n", keyName);
    
    HKEY parentKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Leading Edge", 0, KEY_WRITE, &parentKey) == ERROR_SUCCESS) {
        const char* subkeyName = (cameraIndex == 0) ? "SpoutCam" : (keyName + strlen("Software\\Leading Edge\\"));
        LONG result = RegDeleteKeyA(parentKey, subkeyName);
        RegCloseKey(parentKey);
        
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) {
            LOG("  -> Successfully deleted registry settings\n");
            LOG_FUNC_END("DeleteRegistrySettings");
            return true;
        } else {
            LOG("  -> Failed to delete registry key (error %ld)\n", result);
            LOG_FUNC_END("DeleteRegistrySettings");
            return false;
        }
    }
    LOG("  -> Failed to open parent registry key\n");
    LOG_FUNC_END("DeleteRegistrySettings");
    return false;
}

void CleanupOrphanedCameras()
{
    LOG_FUNC_START("CleanupOrphanedCameras");
    LOG_PERF_START("Cleanup process", cleanupPerf);
    LOG("\n=== Comprehensive cleanup of SpoutCam ===\n");
    
    // Clean up registry settings for cameras 2-8 (potential orphaned ones)
    LOG("Cleaning up potentially orphaned registry settings...\n");
    bool registryCleanupSuccess = true;
    for (int i = 1; i < 8; i++) {
        bool hasSettings = HasCameraSettings(i);
        if (hasSettings) {
            LOG("Cleaning up SpoutCam%d registry settings...\n", i + 1);
            if (!DeleteRegistrySettings(i)) {
                registryCleanupSuccess = false;
            }
        }
    }
    
    char message[512];
    if (registryCleanupSuccess) {
        strcpy_s(message, "Cleanup completed successfully!\n\n");
        strcat_s(message, "[OK] Orphaned registry settings cleaned up\n\n");
        strcat_s(message, "You can now register individual cameras as needed.");
        MessageBox(nullptr, message, "Cleanup Complete", MB_OK | MB_ICONINFORMATION);
    } else {
        strcpy_s(message, "Cleanup had some issues.\n\n");
        strcat_s(message, "Some registry settings couldn't be cleaned.\n");
        strcat_s(message, "Check console for details.");
        MessageBox(nullptr, message, "Cleanup Issues", MB_OK | MB_ICONWARNING);
    }
    
    LOG_PERF_END("Cleanup process", cleanupPerf);
    LOG_FUNC_END("CleanupOrphanedCameras");
}

// Alternative main function for console builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}