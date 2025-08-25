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
#include "DynamicCameraManager.h"

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

// Cache for active filters for performance
std::vector<std::string> g_activeFilters;
bool g_filtersScanned = false;

// Dynamic camera management - now uses DynamicCameraManager
bool g_camerasScanned = false;

// Timer ID for auto-refresh
#define TIMER_AUTO_REFRESH 1001
#define AUTO_REFRESH_INTERVAL_MS 3000  // 3 seconds - balance between responsiveness and performance

// Forward declarations
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AddCameraDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void PopulateCameraList(HWND hListView);
void ScanDynamicCameras();
void RefreshCameraList(HWND hListView);
void AutoRefreshCameraList(HWND hListView);
bool ActivateCameraByName(const std::string& cameraName);
bool DeactivateCameraByName(const std::string& cameraName);
void OpenCameraPropertiesByName(const std::string& cameraName);
bool CreateNewCamera(const char* name);
bool RemoveCameraByName(const std::string& cameraName);
bool RegisterCamera(const char* cameraName);
bool UnregisterCamera(const char* cameraName);  
void OpenCameraProperties(const char* cameraName);

// Legacy index-based wrappers
bool RegisterLegacyCamera(int cameraIndex);
bool UnregisterLegacyCamera(int cameraIndex);  
void OpenLegacyCameraProperties(int cameraIndex);
bool IsRunningAsAdmin();
bool RestartAsAdmin();
void ScanActiveFilters();
bool ReadStringFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* buffer, DWORD bufferSize);
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* value);
bool WriteStringToRegistry(HKEY hKey, const char* subkey, const char* valuename, const char* value);
bool WriteBinaryToRegistry(HKEY hKey, const char* subkey, const char* valuename, const BYTE* data, DWORD dataSize);
void DeleteCameraConfiguration(const char* cameraName);
void DeleteLegacyCameraConfiguration(int cameraIndex);
bool IsCameraActive(const char* cameraName);
bool HasCameraSettings(const char* cameraName);

// Legacy index-based wrappers for backward compatibility
bool IsCameraActiveByIndex(int cameraIndex);
bool HasCameraSettingsByIndex(int cameraIndex);
std::string GenerateDefaultCameraName();

// Camera creation result codes
enum CameraCreationResult {
    CAMERA_CREATE_SUCCESS = 0,
    CAMERA_CREATE_INVALID_NAME = 1,
    CAMERA_CREATE_NAME_EXISTS = 2,
    CAMERA_CREATE_MAX_REACHED = 3,
    CAMERA_CREATE_REGISTRY_ERROR = 4
};

CameraCreationResult CreateNewCameraEx(const char* name);

// Function typedefs for DLL exports
typedef HRESULT (STDAPICALLTYPE *RegisterSingleSpoutCameraFunc)(int cameraIndex);
typedef HRESULT (STDAPICALLTYPE *UnregisterSingleSpoutCameraFunc)(int cameraIndex);
typedef BOOL (STDAPICALLTYPE *GetSpoutCameraNameFunc)(int cameraIndex, char* nameBuffer, int bufferSize);

HINSTANCE g_hInst;


// Optimized function to scan active DirectShow video capture filters
void ScanActiveFilters()
{
    if (g_filtersScanned) {
        return; // Use cached results
    }
    
    g_activeFilters.clear();
    
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
                            g_activeFilters.push_back(std::string(friendlyName));
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

bool WriteStringToRegistry(HKEY hKey, const char* subkey, const char* valuename, const char* value)
{
    HKEY key;
    LONG openResult = RegCreateKeyExA(hKey, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }
    
    LONG setResult = RegSetValueExA(key, valuename, 0, REG_SZ, (const BYTE*)value, (DWORD)strlen(value) + 1);
    bool result = (setResult == ERROR_SUCCESS);
    
    RegCloseKey(key);
    return result;
}

bool WriteBinaryToRegistry(HKEY hKey, const char* subkey, const char* valuename, const BYTE* data, DWORD dataSize)
{
    HKEY key;
    LONG openResult = RegCreateKeyExA(hKey, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }
    
    LONG setResult = RegSetValueExA(key, valuename, 0, REG_BINARY, data, dataSize);
    bool result = (setResult == ERROR_SUCCESS);
    
    RegCloseKey(key);
    return result;
}

void DeleteCameraConfiguration(const char* cameraName)
{
    char keyName[256];
    sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam\\%s", cameraName);
    
    LOG("Deleting configuration for camera '%s' (registry key: %s)\n", cameraName, keyName);
    
    // Delete the entire registry key and all its values
    LONG result = RegDeleteTreeA(HKEY_CURRENT_USER, keyName);
    if (result == ERROR_SUCCESS) {
        LOG("Successfully deleted configuration for camera '%s'\n", cameraName);
    } else if (result == ERROR_FILE_NOT_FOUND) {
        LOG("No configuration found for camera '%s' (key didn't exist)\n", cameraName);
    } else {
        LOG("Failed to delete configuration for camera '%s', error code: %ld\n", cameraName, result);
    }
}

// Legacy function for cleanup (kept for backward compatibility with existing cleanup code)
void DeleteLegacyCameraConfiguration(int cameraIndex)
{
    char keyName[256];
    
    // Build registry key name (match old cam.cpp logic)
    if (cameraIndex == 0) {
        strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
    } else {
        sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
    }
    
    LOG("Deleting legacy configuration for camera %d (registry key: %s)\n", cameraIndex + 1, keyName);
    
    // Delete the entire registry key and all its values
    LONG result = RegDeleteTreeA(HKEY_CURRENT_USER, keyName);
    if (result == ERROR_SUCCESS) {
        LOG("Successfully deleted legacy configuration for camera %d\n", cameraIndex + 1);
    } else if (result == ERROR_FILE_NOT_FOUND) {
        LOG("No legacy configuration found for camera %d (key didn't exist)\n", cameraIndex + 1);
    } else {
        LOG("Failed to delete legacy configuration for camera %d, error code: %ld\n", cameraIndex + 1, result);
    }
}


// Scan for dynamic cameras in registry
void ScanDynamicCameras()
{
    if (g_camerasScanned) {
        return; // Use cached results
    }
    
    // Use DynamicCameraManager to get all cameras
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto cameras = manager->GetAllCameras();
    
    LOG("Found %d dynamic cameras\n", (int)cameras.size());
    
    g_camerasScanned = true;
}

// No longer needed - using fully dynamic system

// Enhanced camera creation with detailed error reporting
CameraCreationResult CreateNewCameraEx(const char* name)
{
    // Validate name
    if (!name || strlen(name) == 0 || strlen(name) > 64) {
        LOG("Invalid camera name\n");
        return CAMERA_CREATE_INVALID_NAME;
    }
    
    // Use DynamicCameraManager to create camera
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    
    // Check if name already exists
    if (manager->GetCamera(name) != nullptr) {
        LOG("Camera name '%s' already exists\n", name);
        return CAMERA_CREATE_NAME_EXISTS;
    }
    
    // Create new camera
    auto camera = manager->CreateCamera(name);
    if (!camera) {
        LOG("Failed to create camera '%s'\n", name);
        return CAMERA_CREATE_REGISTRY_ERROR;
    }
    
    LOG("Created new camera '%s'\n", name);
    
    // Clear cache to refresh
    g_camerasScanned = false;
    
    return CAMERA_CREATE_SUCCESS;
}

// Legacy wrapper function for backward compatibility
bool CreateNewCamera(const char* name)
{
    return CreateNewCameraEx(name) == CAMERA_CREATE_SUCCESS;
}

// Remove a camera and its configuration using dynamic system
bool RemoveCameraByName(const std::string& cameraName)
{
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto camera = manager->GetCamera(cameraName);
    
    if (!camera) {
        LOG("Camera '%s' not found\n", cameraName.c_str());
        return false;
    }
    
    // First deactivate if active
    if (camera->isActive) {
        if (!DeactivateCameraByName(cameraName)) {
            LOG("Failed to deactivate camera '%s' before removal\n", cameraName.c_str());
            // Continue anyway to clean up registry
        }
    }
    
    // Delete from dynamic manager
    if (manager->DeleteCamera(cameraName)) {
        LOG("Removed camera '%s'\n", cameraName.c_str());
        // Clear cache to refresh
        g_camerasScanned = false;
        
        // Force cleanup and reload of the manager instance to ensure persistent removal
        SpoutCam::DynamicCameraManager::Cleanup();
        // The next call to GetInstance() will create a new instance and reload from registry
        
        return true;
    } else {
        LOG("Failed to remove camera '%s'\n", cameraName.c_str());
        return false;
    }
}

bool IsCameraActive(int cameraIndex)
{
    // Get the actual name that the DLL will use for this camera
    char actualCameraName[256];
    
    // Try to load the DLL and get the actual name
    wchar_t dllPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
#ifdef _WIN64
    const wchar_t* subDir = L"SpoutCam64";
    const wchar_t* dllName = L"SpoutCam64.ax";
#else
    const wchar_t* subDir = L"SpoutCam32";  
    const wchar_t* dllName = L"SpoutCam32.ax";
#endif
    
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    HMODULE hDll = LoadLibraryW(dllPath);
    if (hDll) {
        GetSpoutCameraNameFunc getNameFunc = 
            (GetSpoutCameraNameFunc)GetProcAddress(hDll, "GetSpoutCameraName");
        
        if (getNameFunc && getNameFunc(cameraIndex, actualCameraName, sizeof(actualCameraName))) {
            // Use the actual name returned by the DLL
            for (const auto& filter : g_activeFilters) {
                if (filter.find(actualCameraName) != std::string::npos) {
                    FreeLibrary(hDll);
                    return true;
                }
            }
        }
        FreeLibrary(hDll);
    }
    
    // Fallback to old method if DLL is not available
    char searchName[32];
    if (cameraIndex == 0) {
        strcpy_s(searchName, "SpoutCam");  // First camera has no number
    } else {
        sprintf_s(searchName, "SpoutCam%d", cameraIndex + 1);  // SpoutCam2, SpoutCam3, etc.
    }
    
    for (const auto& filter : g_activeFilters) {
        if (filter.find(searchName) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// New name-based version
bool IsCameraActive(const char* cameraName)
{
    if (!cameraName) return false;
    
    // Use the DynamicCameraManager to check if camera is active
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    return manager->IsCameraActive(cameraName);
}

// Legacy wrapper that converts index to name
bool IsCameraActiveByIndex(int cameraIndex)
{
    // Convert index to camera name using legacy naming convention
    char cameraName[32];
    if (cameraIndex == 0) {
        strcpy_s(cameraName, "SpoutCam");
    } else {
        sprintf_s(cameraName, "SpoutCam%d", cameraIndex + 1);
    }
    
    return IsCameraActive(cameraName);
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
    bool hasFps = ReadDwordFromRegistry(HKEY_LOCAL_MACHINE, keyName, "fps", &testValue);
    bool hasResolution = ReadDwordFromRegistry(HKEY_LOCAL_MACHINE, keyName, "resolution", &testValue);
    bool hasMirror = ReadDwordFromRegistry(HKEY_LOCAL_MACHINE, keyName, "mirror", &testValue);
    
    return (hasFps || hasResolution || hasMirror);
}

// New name-based version
bool HasCameraSettings(const char* cameraName)
{
    if (!cameraName) return false;
    
    // Use the DynamicCameraManager to check if camera has settings
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    return manager->CameraHasSettings(cameraName);
}

// Legacy wrapper that converts index to name
bool HasCameraSettingsByIndex(int cameraIndex)
{
    // Convert index to camera name using legacy naming convention
    char cameraName[32];
    if (cameraIndex == 0) {
        strcpy_s(cameraName, "SpoutCam");
    } else {
        sprintf_s(cameraName, "SpoutCam%d", cameraIndex + 1);
    }
    
    return HasCameraSettings(cameraName);
}

// Generate the next available default camera name using dynamic system
std::string GenerateDefaultCameraName()
{
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    return manager->GenerateAvailableName("SpoutCam");
}

void RefreshCameraList(HWND hListView)
{
    g_filtersScanned = false; // Force rescan
    g_camerasScanned = false; // Force rescan of cameras
    
    // Force cleanup and reload of the manager instance to ensure fresh data from registry
    SpoutCam::DynamicCameraManager::Cleanup();
    
    ScanActiveFilters();
    ScanDynamicCameras();
    PopulateCameraList(hListView);
}

// Lighter auto-refresh that only updates if there are actual changes
void AutoRefreshCameraList(HWND hListView)
{
    // Store current camera count and registration states to detect changes
    static size_t lastCameraCount = 0;
    static std::vector<std::pair<std::string, bool>> lastCameraStates;
    
    // Rescan data (but reuse cache where possible)
    ScanActiveFilters();
    ScanDynamicCameras();
    
    // Get current cameras from dynamic manager
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto cameras = manager->GetAllCameras();
    
    // Check if anything changed
    bool needsUpdate = false;
    
    // Check camera count change
    if (cameras.size() != lastCameraCount) {
        needsUpdate = true;
        lastCameraCount = cameras.size();
    }
    
    // Check for changes in camera names or registration states
    if (lastCameraStates.size() != cameras.size()) {
        needsUpdate = true;
    } else {
        for (size_t i = 0; i < cameras.size(); i++) {
            if (i >= lastCameraStates.size() || 
                lastCameraStates[i].first != cameras[i]->name || 
                lastCameraStates[i].second != cameras[i]->isActive) {
                needsUpdate = true;
                break;
            }
        }
    }
    
    // Update cached state
    if (needsUpdate) {
        lastCameraStates.clear();
        for (const auto& camera : cameras) {
            lastCameraStates.push_back({camera->name, camera->isActive});
        }
        
        LOG("Camera state changed, updating list display\n");
        
        // Save focus state before auto-refresh update
        HWND hCurrentFocus = GetFocus();
        
        PopulateCameraList(hListView);
        
        // Restore focus if it was stolen during auto-refresh
        if (hCurrentFocus && GetFocus() != hCurrentFocus) {
            SetFocus(hCurrentFocus);
        }
    }
}

void PopulateCameraList(HWND hListView)
{
    // Save current focus and selection state before refresh
    HWND hCurrentFocus = GetFocus();
    int selectedItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    
    // Prevent UI notifications during update to avoid focus stealing
    SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
    
    // Scan for active filters and dynamic cameras
    ScanActiveFilters();
    ScanDynamicCameras();
    
    // Clear existing items
    ListView_DeleteAllItems(hListView);
    
    // Get cameras from dynamic manager
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto cameras = manager->GetAllCameras();
    
    // Add each dynamic camera to the list
    for (size_t i = 0; i < cameras.size(); i++) {
        const auto& camera = cameras[i];
        
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.lParam = (LPARAM)i; // Store camera index in item data
        lvi.pszText = (LPSTR)camera->name.c_str();
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        
        // Set activation status
        ListView_SetItemText(hListView, itemIndex, 1, (LPSTR)(camera->isActive ? "Active" : "Inactive"));
        
        // Set configuration status based on whether camera has saved settings
        const char* settingsStatus = camera->hasSettings ? "Custom" : "Default";
        ListView_SetItemText(hListView, itemIndex, 2, (LPSTR)settingsStatus);
    }
    
    // If no cameras exist, show a helpful message
    if (cameras.empty()) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0;
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)"No cameras created yet";
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        ListView_SetItemText(hListView, itemIndex, 1, (LPSTR)"Click 'Add New Camera' to start");
        ListView_SetItemText(hListView, itemIndex, 2, (LPSTR)"--");
    }
    
    // Re-enable drawing and restore focus/selection
    SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
    
    // Restore selection if the item still exists
    if (selectedItem >= 0 && selectedItem < ListView_GetItemCount(hListView)) {
        ListView_SetItemState(hListView, selectedItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    
    // Restore focus to previous control if it wasn't the listview
    if (hCurrentFocus && hCurrentFocus != hListView) {
        SetFocus(hCurrentFocus);
    }
    
    // Force listview to redraw with restored state
    InvalidateRect(hListView, NULL, TRUE);
}

// New dynamic camera registration function
bool ActivateCameraByName(const std::string& cameraName)
{
    LOG("Activating dynamic camera '%s'\n", cameraName.c_str());
    
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto camera = manager->GetCamera(cameraName);
    
    if (!camera) {
        LOG("Camera '%s' not found\n", cameraName.c_str());
        return false;
    }
    
    // Use the DLL registration system
    wchar_t dllPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
#ifdef _WIN64
    const wchar_t* subDir = L"SpoutCam64";
    const wchar_t* dllName = L"SpoutCam64.ax";
#else
    const wchar_t* subDir = L"SpoutCam32";
    const wchar_t* dllName = L"SpoutCam32.ax";
#endif
    
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    LOG("Loading SpoutCam DLL: %ls\n", dllPath);
    
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        LOG("Failed to load DLL: %ls\n", dllPath);
        return false;
    }
    
    // Get the specific camera registration function
    typedef HRESULT (STDAPICALLTYPE *RegisterCameraByNameFunc)(const char*);
    RegisterCameraByNameFunc registerFunc = 
        (RegisterCameraByNameFunc)GetProcAddress(hDll, "RegisterCameraByName");
    
    HRESULT hr = E_FAIL;
    if (registerFunc) {
        hr = registerFunc(cameraName.c_str());
        LOG("Called RegisterCameraByName('%s') -> 0x%08X\n", cameraName.c_str(), hr);
    } else {
        LOG("Failed to find RegisterCameraByName function in DLL\n");
    }
    
    FreeLibrary(hDll);
    
    bool success = SUCCEEDED(hr);
    if (success) {
        manager->SetCameraActive(cameraName, true);
        LOG("Registration result: 0x%08X (SUCCESS)\n", hr);
        LOG("Dynamic camera '%s' registered successfully\n", cameraName.c_str());
    } else {
        LOG("Registration result: 0x%08X (FAILED)\n", hr);
        LOG("Failed to register dynamic camera '%s'\n", cameraName.c_str());
    }
    
    return success;
}


// Legacy camera registration function (kept for compatibility)
bool RegisterLegacyCamera(int cameraIndex)
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

// New dynamic camera unregistration function
bool DeactivateCameraByName(const std::string& cameraName)
{
    LOG("Deactivating dynamic camera '%s'\n", cameraName.c_str());
    
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto camera = manager->GetCamera(cameraName);
    
    if (!camera) {
        LOG("Camera '%s' not found\n", cameraName.c_str());
        return false;
    }
    
    // Use the DLL unregistration system
    wchar_t dllPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    
#ifdef _WIN64
    const wchar_t* subDir = L"SpoutCam64";
    const wchar_t* dllName = L"SpoutCam64.ax";
#else
    const wchar_t* subDir = L"SpoutCam32";
    const wchar_t* dllName = L"SpoutCam32.ax";
#endif
    
    swprintf_s(dllPath, MAX_PATH, L"%s\\%s\\%s", exePath, subDir, dllName);
    
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        LOG("Failed to load DLL for unregistration\n");
        return false;
    }
    
    // Get the specific camera unregistration function
    typedef HRESULT (STDAPICALLTYPE *UnregisterCameraByNameFunc)(const char*);
    UnregisterCameraByNameFunc unregisterFunc = 
        (UnregisterCameraByNameFunc)GetProcAddress(hDll, "UnregisterCameraByName");
    
    HRESULT hr = E_FAIL;
    if (unregisterFunc) {
        hr = unregisterFunc(cameraName.c_str());
        LOG("Called UnregisterCameraByName('%s') -> 0x%08X\n", cameraName.c_str(), hr);
    } else {
        LOG("Failed to find UnregisterCameraByName function in DLL\n");
    }
    
    FreeLibrary(hDll);
    
    bool success = SUCCEEDED(hr);
    if (success) {
        manager->SetCameraActive(cameraName, false);
        LOG("Dynamic camera '%s' unregistered successfully\n", cameraName.c_str());
    } else {
        LOG("Failed to unregister dynamic camera '%s'\n", cameraName.c_str());
    }
    
    return success;
}


// Legacy camera unregistration function (kept for compatibility)
bool UnregisterLegacyCamera(int cameraIndex)
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

// Open camera properties using dynamic system
void OpenCameraPropertiesByName(const std::string& cameraName)
{
    LOG("Opening properties for dynamic camera '%s'\n", cameraName.c_str());
    
    // NOTE: When properties are saved in the properties dialog (camprops.cpp),
    // that dialog should call manager->SetCameraHasSettings(cameraName, true)
    // to mark this camera as having custom settings
    
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
    
    // Store the camera name in registry for the properties dialog to use
    HKEY hKey;
    LONG result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", 0, NULL, 
                                  REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    
    if (result == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "SelectedCameraName", 0, REG_SZ, (BYTE*)cameraName.c_str(), (DWORD)cameraName.length() + 1);
        RegCloseKey(hKey);
        LOG("Stored camera name '%s' in registry\n", cameraName.c_str());
    } else {
        LOG("Failed to store camera info in registry\n");
    }
    
    // Use camera name as parameter
    std::wstring wCameraName(cameraName.begin(), cameraName.end());
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = cmdPath;
    sei.lpParameters = wCameraName.c_str();
    sei.nShow = SW_SHOW;
    
    if (!ShellExecuteExW(&sei)) {
        MessageBox(nullptr, "Failed to open camera properties", "Error", MB_OK | MB_ICONERROR);
    }
}

// Legacy camera properties function (kept for compatibility)  
void OpenLegacyCameraProperties(int cameraIndex)
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

// Dialog procedure for Add New Camera dialog
INT_PTR CALLBACK AddCameraDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
            {
                // Generate and set the default camera name
                std::string defaultName = GenerateDefaultCameraName();
                SetDlgItemTextA(hDlg, IDC_CAMERA_NAME_EDIT, defaultName.c_str());
                
                // Select all text in the edit control for easy editing
                HWND hEdit = GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT);
                SendMessage(hEdit, EM_SETSEL, 0, -1);
                
                // Set focus to the name edit control
                SetFocus(hEdit);
                return FALSE; // Return FALSE since we set focus ourselves
            }
            
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                
                switch (wmId) {
                    case IDOK:
                        {
                            char cameraName[256];
                            GetDlgItemTextA(hDlg, IDC_CAMERA_NAME_EDIT, cameraName, sizeof(cameraName));
                            
                            // Validate camera name
                            if (strlen(cameraName) == 0) {
                                MessageBox(hDlg, "Please enter a camera name.", "Invalid Name", MB_OK | MB_ICONWARNING);
                                SetFocus(GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT));
                                return TRUE;
                            }
                            
                            if (strlen(cameraName) > 64) {
                                MessageBox(hDlg, "Camera name is too long. Please use 64 characters or less.", "Invalid Name", MB_OK | MB_ICONWARNING);
                                SetFocus(GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT));
                                return TRUE;
                            }
                            
                            // Check for invalid characters (basic validation)
                            for (const char* p = cameraName; *p; p++) {
                                if (*p == '\\' || *p == '/' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
                                    MessageBox(hDlg, "Camera name contains invalid characters. Please avoid: \\ / : * ? \" < > |", "Invalid Name", MB_OK | MB_ICONWARNING);
                                    SetFocus(GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT));
                                    return TRUE;
                                }
                            }
                            
                            // Try to create the camera with detailed error reporting
                            CameraCreationResult result = CreateNewCameraEx(cameraName);
                            if (result == CAMERA_CREATE_SUCCESS) {
                                EndDialog(hDlg, IDOK);
                                return TRUE;
                            } else {
                                // Show specific error message based on the result
                                const char* errorMessage;
                                const char* errorTitle;
                                
                                switch (result) {
                                    case CAMERA_CREATE_NAME_EXISTS:
                                        errorMessage = "A camera with this name already exists. Please choose a different name.";
                                        errorTitle = "Name Already Exists";
                                        break;
                                    case CAMERA_CREATE_MAX_REACHED:
                                        errorMessage = "Maximum number of cameras (8) has been reached.\n\nTo add a new camera, please remove an existing camera first.";
                                        errorTitle = "Maximum Cameras Reached";
                                        break;
                                    case CAMERA_CREATE_INVALID_NAME:
                                        errorMessage = "Camera name is invalid. Please use a name between 1-64 characters.";
                                        errorTitle = "Invalid Name";
                                        break;
                                    case CAMERA_CREATE_REGISTRY_ERROR:
                                        errorMessage = "Failed to save camera configuration to registry. Please check permissions.";
                                        errorTitle = "Registry Error";
                                        break;
                                    default:
                                        errorMessage = "Failed to create camera due to an unknown error.";
                                        errorTitle = "Creation Failed";
                                        break;
                                }
                                
                                MessageBox(hDlg, errorMessage, errorTitle, MB_OK | MB_ICONERROR);
                                SetFocus(GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT));
                                return TRUE;
                            }
                        }
                        break;
                        
                    case IDCANCEL:
                        EndDialog(hDlg, IDCANCEL);
                        return TRUE;
                }
            }
            break;
    }
    
    return FALSE;
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
                
                // Start auto-refresh timer
                SetTimer(hDlg, TIMER_AUTO_REFRESH, AUTO_REFRESH_INTERVAL_MS, NULL);
                LOG("Started auto-refresh timer with %d ms interval\n", AUTO_REFRESH_INTERVAL_MS);
                
                return (INT_PTR)TRUE;
            }
            
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                
                switch (wmId) {
                    case IDC_ADD_NEW_CAMERA:
                        {
                            // Generate default camera name and create camera directly
                            std::string defaultName = GenerateDefaultCameraName();
                            CameraCreationResult result = CreateNewCameraEx(defaultName.c_str());
                            
                            if (result == CAMERA_CREATE_SUCCESS) {
                                // Camera was created successfully, automatically register it
                                LOG("New camera '%s' added successfully\n", defaultName.c_str());
                                
                                // Automatically activate the new camera so property pages work immediately
                                if (ActivateCameraByName(defaultName)) {
                                    LOG("New camera '%s' auto-activated successfully\n", defaultName.c_str());
                                } else {
                                    LOG("Failed to auto-activate new camera '%s'\n", defaultName.c_str());
                                }
                                
                                // Refresh the list to show the registered state
                                RefreshCameraList(hListView);
                                
                                // Get cameras and find the newly created one
                                auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                                auto cameras = manager->GetAllCameras();
                                for (size_t i = 0; i < cameras.size(); i++) {
                                    if (cameras[i]->name == defaultName) {
                                        // Select the new camera in the list
                                        ListView_SetItemState(hListView, (int)i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                                        
                                        // Open its properties dialog
                                        OpenCameraPropertiesByName(defaultName);
                                        break;
                                    }
                                }
                            } else {
                                // Show error message based on the result
                                const char* errorMessage;
                                const char* errorTitle;
                                
                                switch (result) {
                                    case CAMERA_CREATE_REGISTRY_ERROR:
                                        errorMessage = "Failed to save camera configuration to registry.";
                                        errorTitle = "Registry Error";
                                        break;
                                    default:
                                        errorMessage = "Failed to create camera due to an unknown error.";
                                        errorTitle = "Creation Failed";
                                        break;
                                }
                                
                                MessageBox(hDlg, errorMessage, errorTitle, MB_OK | MB_ICONERROR);
                                LOG("Failed to create camera: %s\n", errorMessage);
                            }
                        }
                        break;
                        
                    case IDC_REGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                            auto cameras = manager->GetAllCameras();
                            
                            if (selected != -1 && selected < (int)cameras.size()) {
                                const std::string cameraName = cameras[selected]->name; // Copy name
                                if (ActivateCameraByName(cameraName)) {
                                    RefreshCameraList(hListView);
                                    LOG("Camera '%s' activated successfully\n", cameraName.c_str());
                                } else {
                                    LOG("Failed to activate camera '%s'\n", cameraName.c_str());
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to activate.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_UNREGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                            auto cameras = manager->GetAllCameras();
                            
                            if (selected != -1 && selected < (int)cameras.size()) {
                                const std::string cameraName = cameras[selected]->name; // Copy name
                                if (DeactivateCameraByName(cameraName)) {
                                    RefreshCameraList(hListView);
                                    LOG("Camera '%s' deactivated successfully\n", cameraName.c_str());
                                } else {
                                    LOG("Failed to deactivate camera '%s'\n", cameraName.c_str());
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to deactivate.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_REREGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                            auto cameras = manager->GetAllCameras();
                            
                            if (selected != -1 && selected < (int)cameras.size()) {
                                const std::string cameraName = cameras[selected]->name; // Copy name
                                
                                LOG("Reactivating camera '%s'\n", cameraName.c_str());
                                
                                // First deactivate
                                bool deactivateSuccess = DeactivateCameraByName(cameraName);
                                
                                // Brief pause to ensure deactivation is complete
                                Sleep(500);
                                
                                // Then activate again
                                bool activateSuccess = ActivateCameraByName(cameraName);
                                
                                if (deactivateSuccess && activateSuccess) {
                                    RefreshCameraList(hListView);
                                    LOG("Camera '%s' reactivated successfully\n", cameraName.c_str());
                                    
                                    // Show success message
                                    char successMsg[256];
                                    sprintf_s(successMsg, "Camera '%s' has been successfully reactivated.\n\nThe camera is now ready to use in video applications.", cameraName.c_str());
                                    MessageBox(hDlg, successMsg, "Reactivation Complete", MB_OK | MB_ICONINFORMATION);
                                } else {
                                    RefreshCameraList(hListView);
                                    LOG("Failed to reactivate camera '%s' (deactivate: %s, activate: %s)\n", 
                                        cameraName.c_str(), 
                                        deactivateSuccess ? "success" : "failed",
                                        activateSuccess ? "success" : "failed");
                                    
                                    char errorMsg[256];
                                    sprintf_s(errorMsg, "Failed to reactivate camera '%s'.\n\nCheck the debug console for details.", cameraName.c_str());
                                    MessageBox(hDlg, errorMsg, "Reactivation Failed", MB_OK | MB_ICONERROR);
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to reactivate.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_REMOVE_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                            auto cameras = manager->GetAllCameras();
                            
                            if (selected != -1 && selected < (int)cameras.size()) {
                                const std::string cameraName = cameras[selected]->name; // Copy name
                                
                                // Validate camera name to prevent crashes
                                if (cameraName.empty()) {
                                    LOG("ERROR: Attempting to remove camera with empty name at index %d\n", selected);
                                    char errorMsg[256];
                                    sprintf_s(errorMsg, "Cannot remove camera at position %d - corrupted camera name.\n\nWould you like to force remove this corrupted entry?", selected + 1);
                                    int result = MessageBox(hDlg, errorMsg, "Corrupted Camera Entry", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                    
                                    if (result == IDYES) {
                                        // Force remove the corrupted entry
                                        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                                        if (manager->DeleteCamera("")) {  // Try to delete with empty name
                                            RefreshCameraList(hListView);
                                            LOG("Corrupted camera entry at index %d force-removed\n", selected);
                                        } else {
                                            LOG("Failed to force-remove corrupted camera entry at index %d\n", selected);
                                        }
                                    }
                                    break;
                                }
                                
                                char confirmMsg[512];
                                sprintf_s(confirmMsg, "Are you sure you want to remove camera '%s'?\n\nThis will permanently delete the camera and all its settings.", cameraName.c_str());
                                
                                int result = MessageBox(hDlg, confirmMsg, "Confirm Removal", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                if (result == IDYES) {
                                    if (RemoveCameraByName(cameraName)) {
                                        RefreshCameraList(hListView);
                                        LOG("Camera '%s' removed successfully\n", cameraName.c_str());
                                    } else {
                                        LOG("Failed to remove camera '%s'\n", cameraName.c_str());
                                    }
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to remove.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_REFRESH:
                        RefreshCameraList(hListView);
                        break;
                        
                    case IDC_CLEANUP:
                        {
                            // Cleanup orphaned camera registrations and configurations for all 8 cameras
                            int result = MessageBox(hDlg, 
                                "This will unregister ALL SpoutCam cameras (1-8) and delete all their configurations.\n\n"
                                "WARNING: This will permanently remove all camera settings (FPS, resolution, sender names, etc.)\n\n"
                                "Are you sure you want to proceed?", 
                                "Cleanup All Cameras & Configurations", 
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                
                            if (result == IDYES) {
                                LOG("Starting complete cleanup of all SpoutCam cameras and configurations...\n");
                                int unregisterSuccessCount = 0;
                                int unregisterFailCount = 0;
                                int configDeletedCount = 0;
                                
                                // Unregister all 8 legacy cameras and delete their configurations
                                for (int i = 0; i < 8; i++) {
                                    LOG("Cleaning up SpoutCam%d...\n", i + 1);
                                    
                                    // Unregister the camera
                                    if (UnregisterLegacyCamera(i)) {
                                        unregisterSuccessCount++;
                                        LOG("SpoutCam%d unregistered successfully\n", i + 1);
                                    } else {
                                        unregisterFailCount++;
                                        LOG("Failed to unregister SpoutCam%d\n", i + 1);
                                    }
                                    
                                    // Delete the camera configuration
                                    if (HasCameraSettings(i)) {
                                        DeleteLegacyCameraConfiguration(i);
                                        configDeletedCount++;
                                    }
                                }
                                
                                // Also clean up all dynamic cameras and their settings
                                auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                                auto dynamicCameras = manager->GetAllCameras();
                                for (auto camera : dynamicCameras) {
                                    LOG("Cleaning up dynamic camera '%s'...\n", camera->name.c_str());
                                    
                                    // Deactivate DirectShow filter if active
                                    if (camera->isActive) {
                                        if (DeactivateCameraByName(camera->name)) {
                                            unregisterSuccessCount++;
                                        } else {
                                            unregisterFailCount++;
                                        }
                                    }
                                    
                                    // Delete all registry data for this camera
                                    if (manager->DeleteCameraFromRegistry(camera->name)) {
                                        configDeletedCount++;
                                    }
                                }
                                
                                // Force refresh to show updated status (RefreshCameraList handles cleanup internally)
                                g_filtersScanned = false;
                                RefreshCameraList(hListView);
                                
                                // Show summary
                                char summaryMsg[512];
                                sprintf_s(summaryMsg, 
                                    "Complete cleanup finished!\n\n"
                                    "Camera Registrations:\n"
                                    "  Successfully unregistered: %d cameras\n"
                                    "  Failed to unregister: %d cameras\n\n"
                                    "Camera Configurations:\n"
                                    "  Deleted configurations: %d cameras\n\n"
                                    "All SpoutCam registrations and configurations have been cleaned up.",
                                    unregisterSuccessCount, unregisterFailCount, configDeletedCount);
                                MessageBox(hDlg, summaryMsg, "Complete Cleanup Finished", MB_OK | MB_ICONINFORMATION);
                                
                                LOG("Complete cleanup finished: %d unregistered, %d failed, %d configs deleted\n", 
                                    unregisterSuccessCount, unregisterFailCount, configDeletedCount);
                            } else {
                                LOG("Complete cleanup cancelled by user\n");
                            }
                        }
                        break;
                        
                    case IDC_CONFIGURE_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
                            auto cameras = manager->GetAllCameras();
                            
                            if (selected != -1 && selected < (int)cameras.size()) {
                                const std::string cameraName = cameras[selected]->name; // Copy name
                                OpenCameraPropertiesByName(cameraName);
                            } else if (selected == -1) {
                                MessageBox(hDlg, "Please select a camera to configure.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            } else {
                                MessageBox(hDlg, "Please add a camera first using 'Add New Camera'.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
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
            
        case WM_TIMER:
            {
                if (wParam == TIMER_AUTO_REFRESH) {
                    // Perform auto-refresh - but only if dialog is visible and not in active use
                    if (IsWindowVisible(hDlg)) {
                        // Check if user is actively interacting (mouse captured or key pressed recently)
                        HWND hFocused = GetFocus();
                        HWND hCapture = GetCapture();
                        
                        // Only auto-refresh if no controls have capture and we're not in the middle of user interaction
                        if (!hCapture) {
                            AutoRefreshCameraList(hListView);
                        }
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            {
                // Clean up timer
                KillTimer(hDlg, TIMER_AUTO_REFRESH);
                LOG("Stopped auto-refresh timer\n");
            }
            break;
    }
    
    return (INT_PTR)FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetupConsole();
    LOG("SpoutCam Settings - Camera Management Interface\n");
    
    // Check for command line arguments
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        LOG("Command line arguments: %s\n", lpCmdLine);
        
        // Check for cleanup argument
        if (strstr(lpCmdLine, "--cleanup-all") != nullptr) {
            LOG("Running in cleanup mode from command line...\n");
            
            // Perform the same cleanup as the UI cleanup button
            int unregisterSuccessCount = 0;
            int unregisterFailCount = 0;
            int configDeletedCount = 0;
            
            LOG("Starting complete cleanup of all SpoutCam cameras and configurations...\n");
            
            // Unregister all 8 legacy cameras and delete their configurations
            for (int i = 0; i < 8; i++) {
                LOG("Cleaning up SpoutCam%d...\n", i + 1);
                
                // Unregister the camera
                if (UnregisterLegacyCamera(i)) {
                    unregisterSuccessCount++;
                    LOG("SpoutCam%d unregistered successfully\n", i + 1);
                } else {
                    unregisterFailCount++;
                    LOG("Failed to unregister SpoutCam%d\n", i + 1);
                }
                
                // Delete the camera configuration
                if (HasCameraSettings(i)) {
                    DeleteLegacyCameraConfiguration(i);
                    configDeletedCount++;
                }
            }
            
            // Also clean up all dynamic cameras and their settings
            auto manager = SpoutCam::DynamicCameraManager::GetInstance();
            auto dynamicCameras = manager->GetAllCameras();
            for (auto camera : dynamicCameras) {
                LOG("Cleaning up dynamic camera '%s'...\n", camera->name.c_str());
                
                // Deactivate DirectShow filter if active
                if (camera->isActive) {
                    if (DeactivateCameraByName(camera->name)) {
                        unregisterSuccessCount++;
                    } else {
                        unregisterFailCount++;
                    }
                }
                
                // Delete all registry data for this camera
                if (manager->DeleteCameraFromRegistry(camera->name)) {
                    configDeletedCount++;
                }
            }
            
            // Clear and reload manager to reflect deletions
            SpoutCam::DynamicCameraManager::Cleanup();
            
            LOG("Complete cleanup finished: %d unregistered, %d failed, %d configs deleted\n", 
                unregisterSuccessCount, unregisterFailCount, configDeletedCount);
            
            // Return success/failure code
            return (unregisterFailCount > 0) ? 1 : 0;
        }
    }
    
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