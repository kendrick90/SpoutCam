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

// Dynamic camera management structures
struct DynamicCamera {
    std::string name;
    int slotIndex;  // Maps to existing DLL camera slot (0-3)
    GUID clsid;
    GUID propPageClsid;
    bool isRegistered;
    bool hasSettings;
};

std::vector<DynamicCamera> g_dynamicCameras;
bool g_camerasScanned = false;

// Predefined camera configurations from DLL (matching dll.cpp)
struct StaticCameraConfig {
    const char* defaultName;
    GUID clsid;
    GUID propPageClsid;
#define MAX_DYNAMIC_CAMERAS 8

} g_StaticCameraConfigs[MAX_DYNAMIC_CAMERAS] = {
    {
        "SpoutCam",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x3f}}
    },
    {
        "SpoutCam2",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x34}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x40}}
    },
    {
        "SpoutCam3",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x35}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x41}}
    },
    {
        "SpoutCam4",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x36}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x42}}
    },
    {
        "SpoutCam5",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x37}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x43}}
    },
    {
        "SpoutCam6",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x38}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x44}}
    },
    {
        "SpoutCam7",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x39}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x45}}
    },
    {
        "SpoutCam8",
        {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x3a}},
        {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x46}}
    }
};

// Forward declarations
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AddCameraDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void PopulateCameraList(HWND hListView);
void ScanDynamicCameras();
void RefreshCameraList(HWND hListView);
bool RegisterCamera(const DynamicCamera& camera);
bool UnregisterCamera(const DynamicCamera& camera);
void OpenCameraProperties(const DynamicCamera& camera);
bool CreateNewCamera(const char* name);
bool RemoveCamera(const DynamicCamera& camera);
int FindAvailableSlot();
bool RegisterLegacyCamera(int cameraIndex);
bool UnregisterLegacyCamera(int cameraIndex);  
void OpenLegacyCameraProperties(int cameraIndex);
bool IsRunningAsAdmin();
bool RestartAsAdmin();
void ScanRegisteredFilters();
bool ReadStringFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* buffer, DWORD bufferSize);
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* value);
bool WriteStringToRegistry(HKEY hKey, const char* subkey, const char* valuename, const char* value);
bool WriteBinaryToRegistry(HKEY hKey, const char* subkey, const char* valuename, const BYTE* data, DWORD dataSize);
void DeleteCameraConfiguration(const char* cameraName);
void DeleteLegacyCameraConfiguration(int cameraIndex);

// Function typedefs for DLL exports
typedef HRESULT (STDAPICALLTYPE *RegisterSingleSpoutCameraFunc)(int cameraIndex);
typedef HRESULT (STDAPICALLTYPE *UnregisterSingleSpoutCameraFunc)(int cameraIndex);

HINSTANCE g_hInst;

#define MAX_CAMERAS 32  // Increased limit for dynamic cameras

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
    sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam\\Cameras\\%s", cameraName);
    
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
    
    g_dynamicCameras.clear();
    
    const char* camerasPath = "Software\\Leading Edge\\SpoutCam\\Cameras";
    HKEY camerasKey;
    
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, camerasPath, 0, KEY_READ, &camerasKey);
    if (result != ERROR_SUCCESS) {
        // No cameras registered yet - this is normal
        g_camerasScanned = true;
        return;
    }
    
    DWORD subkeyCount = 0;
    DWORD maxSubkeyLen = 0;
    result = RegQueryInfoKeyA(camerasKey, nullptr, nullptr, nullptr, &subkeyCount, &maxSubkeyLen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    
    if (result == ERROR_SUCCESS) {
        char* subkeyName = new char[maxSubkeyLen + 1];
        
        for (DWORD i = 0; i < subkeyCount; i++) {
            DWORD subkeyNameLen = maxSubkeyLen + 1;
            if (RegEnumKeyExA(camerasKey, i, subkeyName, &subkeyNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                DynamicCamera camera;
                camera.name = subkeyName;
                
                char cameraPath[512];
                sprintf_s(cameraPath, "%s\\%s", camerasPath, subkeyName);
                
                // Read slot index from registry
                char slotStr[16];
                if (ReadStringFromRegistry(HKEY_CURRENT_USER, cameraPath, "slotIndex", slotStr, sizeof(slotStr))) {
                    camera.slotIndex = atoi(slotStr);
                    
                    // Validate slot index and use corresponding GUIDs
                    if (camera.slotIndex >= 0 && camera.slotIndex < MAX_DYNAMIC_CAMERAS) {
                        camera.clsid = g_StaticCameraConfigs[camera.slotIndex].clsid;
                        camera.propPageClsid = g_StaticCameraConfigs[camera.slotIndex].propPageClsid;
                    } else {
                        LOG("Invalid slot index %d for camera '%s', using slot 0\n", camera.slotIndex, subkeyName);
                        camera.slotIndex = 0;
                        camera.clsid = g_StaticCameraConfigs[0].clsid;
                        camera.propPageClsid = g_StaticCameraConfigs[0].propPageClsid;
                    }
                } else {
                    // No slot index found, assign available slot
                    camera.slotIndex = FindAvailableSlot();
                    if (camera.slotIndex == -1) {
                        camera.slotIndex = 0; // Fallback to slot 0
                    }
                    camera.clsid = g_StaticCameraConfigs[camera.slotIndex].clsid;
                    camera.propPageClsid = g_StaticCameraConfigs[camera.slotIndex].propPageClsid;
                    
                    // Save the slot index for next time
                    char slotStr[16];
                    sprintf_s(slotStr, "%d", camera.slotIndex);
                    WriteStringToRegistry(HKEY_CURRENT_USER, cameraPath, "slotIndex", slotStr);
                }
                
                // Check if camera is registered (by searching DirectShow filters using the DLL's default name)
                camera.isRegistered = false;
                const char* dllCameraName = g_StaticCameraConfigs[camera.slotIndex].defaultName;
                for (const auto& filter : g_registeredFilters) {
                    if (filter.find(dllCameraName) != std::string::npos) {
                        camera.isRegistered = true;
                        break;
                    }
                }
                
                // Check if camera has settings using the legacy registry path
                char legacyPath[256];
                if (camera.slotIndex == 0) {
                    strcpy_s(legacyPath, "Software\\Leading Edge\\SpoutCam");
                } else {
                    sprintf_s(legacyPath, "Software\\Leading Edge\\SpoutCam%d", camera.slotIndex + 1);
                }
                
                DWORD testValue;
                camera.hasSettings = ReadDwordFromRegistry(HKEY_CURRENT_USER, legacyPath, "fps", &testValue) ||
                                   ReadDwordFromRegistry(HKEY_CURRENT_USER, legacyPath, "resolution", &testValue) ||
                                   ReadDwordFromRegistry(HKEY_CURRENT_USER, legacyPath, "mirror", &testValue);
                
                g_dynamicCameras.push_back(camera);
            }
        }
        delete[] subkeyName;
    }
    
    RegCloseKey(camerasKey);
    g_camerasScanned = true;
}

// Find an available slot for a new camera
int FindAvailableSlot()
{
    bool usedSlots[MAX_DYNAMIC_CAMERAS] = {false};
    
    // Mark slots that are already in use
    for (const auto& camera : g_dynamicCameras) {
        if (camera.slotIndex >= 0 && camera.slotIndex < MAX_DYNAMIC_CAMERAS) {
            usedSlots[camera.slotIndex] = true;
        }
    }
    
    // Find first available slot
    for (int i = 0; i < MAX_DYNAMIC_CAMERAS; i++) {
        if (!usedSlots[i]) {
            return i;
        }
    }
    
    return -1; // No slots available
}

// Create a new camera with the given name
bool CreateNewCamera(const char* name)
{
    // Validate name
    if (!name || strlen(name) == 0 || strlen(name) > 64) {
        LOG("Invalid camera name\n");
        return false;
    }
    
    // Check if name already exists
    for (const auto& camera : g_dynamicCameras) {
        if (camera.name == name) {
            LOG("Camera name '%s' already exists\n", name);
            return false;
        }
    }
    
    // Find available slot
    int availableSlot = FindAvailableSlot();
    if (availableSlot == -1) {
        LOG("No available slots for new camera (maximum %d cameras supported)\n", MAX_DYNAMIC_CAMERAS);
        return false;
    }
    
    // Create new camera entry using existing DLL slot
    DynamicCamera newCamera;
    newCamera.name = name;
    newCamera.slotIndex = availableSlot;
    newCamera.clsid = g_StaticCameraConfigs[availableSlot].clsid;
    newCamera.propPageClsid = g_StaticCameraConfigs[availableSlot].propPageClsid;
    newCamera.isRegistered = false;
    newCamera.hasSettings = false;
    
    // Save to registry
    char cameraPath[512];
    sprintf_s(cameraPath, "Software\\Leading Edge\\SpoutCam\\Cameras\\%s", name);
    
    // Store the camera info
    if (!WriteStringToRegistry(HKEY_CURRENT_USER, cameraPath, "name", name)) {
        LOG("Failed to write camera name to registry\n");
        return false;
    }
    
    // Store the slot index
    char slotStr[16];
    sprintf_s(slotStr, "%d", availableSlot);
    if (!WriteStringToRegistry(HKEY_CURRENT_USER, cameraPath, "slotIndex", slotStr)) {
        LOG("Failed to write slot index to registry\n");
        return false;
    }
    
    // Add to cache
    g_dynamicCameras.push_back(newCamera);
    
    LOG("Created new camera '%s' using slot %d\n", name, availableSlot);
    return true;
}

// Remove a camera and its configuration
bool RemoveCamera(const DynamicCamera& camera)
{
    // First unregister if registered
    if (camera.isRegistered) {
        if (!UnregisterCamera(camera)) {
            LOG("Failed to unregister camera '%s' before removal\n", camera.name.c_str());
            // Continue anyway to clean up registry
        }
    }
    
    // Delete registry configuration
    DeleteCameraConfiguration(camera.name.c_str());
    
    // Remove from cache
    auto it = std::find_if(g_dynamicCameras.begin(), g_dynamicCameras.end(),
                          [&camera](const DynamicCamera& c) { return c.name == camera.name; });
    if (it != g_dynamicCameras.end()) {
        g_dynamicCameras.erase(it);
    }
    
    LOG("Removed camera '%s'\n", camera.name.c_str());
    return true;
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
    g_camerasScanned = false; // Force rescan of cameras
    ScanRegisteredFilters();
    ScanDynamicCameras();
    PopulateCameraList(hListView);
}

void PopulateCameraList(HWND hListView)
{
    // Scan for registered filters and dynamic cameras
    ScanRegisteredFilters();
    ScanDynamicCameras();
    
    // Clear existing items
    ListView_DeleteAllItems(hListView);
    
    // Add each dynamic camera to the list
    for (size_t i = 0; i < g_dynamicCameras.size(); i++) {
        const auto& camera = g_dynamicCameras[i];
        
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.lParam = (LPARAM)i; // Store camera index in item data
        lvi.pszText = (LPSTR)camera.name.c_str();
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        
        // Set registration status
        ListView_SetItemText(hListView, itemIndex, 1, (LPSTR)(camera.isRegistered ? "Registered" : "Not Registered"));
        
        // Set configuration status
        ListView_SetItemText(hListView, itemIndex, 2, (LPSTR)(camera.hasSettings ? "Configured" : "Default"));
    }
    
    // If no cameras exist, show a helpful message
    if (g_dynamicCameras.empty()) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0;
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)"No cameras created yet";
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        ListView_SetItemText(hListView, itemIndex, 1, (LPSTR)"Click 'Add New Camera' to start");
        ListView_SetItemText(hListView, itemIndex, 2, (LPSTR)"--");
    }
}

// New dynamic camera registration function - maps to existing DLL slots
bool RegisterCamera(const DynamicCamera& camera)
{
    LOG("Registering dynamic camera '%s' using slot %d\n", camera.name.c_str(), camera.slotIndex);
    
    // Use the existing legacy registration system with the camera's slot
    bool success = RegisterLegacyCamera(camera.slotIndex);
    
    if (success) {
        // Mark as registered in cache
        auto it = std::find_if(g_dynamicCameras.begin(), g_dynamicCameras.end(),
                              [&camera](DynamicCamera& c) { return c.name == camera.name; });
        if (it != g_dynamicCameras.end()) {
            it->isRegistered = true;
        }
        LOG("Dynamic camera '%s' registered successfully using slot %d\n", camera.name.c_str(), camera.slotIndex);
    } else {
        LOG("Failed to register dynamic camera '%s' using slot %d\n", camera.name.c_str(), camera.slotIndex);
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

// New dynamic camera unregistration function - maps to existing DLL slots
bool UnregisterCamera(const DynamicCamera& camera)
{
    LOG("Unregistering dynamic camera '%s' using slot %d\n", camera.name.c_str(), camera.slotIndex);
    
    // Use the existing legacy unregistration system with the camera's slot
    bool success = UnregisterLegacyCamera(camera.slotIndex);
    
    if (success) {
        // Mark as unregistered in cache
        auto it = std::find_if(g_dynamicCameras.begin(), g_dynamicCameras.end(),
                              [&camera](DynamicCamera& c) { return c.name == camera.name; });
        if (it != g_dynamicCameras.end()) {
            it->isRegistered = false;
        }
        LOG("Dynamic camera '%s' unregistered successfully using slot %d\n", camera.name.c_str(), camera.slotIndex);
    } else {
        LOG("Failed to unregister dynamic camera '%s' using slot %d\n", camera.name.c_str(), camera.slotIndex);
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

// New dynamic camera properties function
void OpenCameraProperties(const DynamicCamera& camera)
{
    LOG("Opening properties for dynamic camera '%s' using slot %d\n", camera.name.c_str(), camera.slotIndex);
    
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
    
    // Create a temporary file with both camera index and name
    char tempFilePath[MAX_PATH];
    sprintf_s(tempFilePath, "%S\\camera_info.tmp", exePath);
    
    FILE* tempFile = nullptr;
    if (fopen_s(&tempFile, tempFilePath, "w") == 0 && tempFile) {
        fprintf(tempFile, "cameraIndex=%d\n", camera.slotIndex);
        fprintf(tempFile, "cameraName=%s\n", camera.name.c_str());
        fclose(tempFile);
        LOG("Created camera info file with index %d and name '%s'\n", camera.slotIndex, camera.name.c_str());
    } else {
        LOG("Failed to create camera info file, falling back to legacy method\n");
        // Fallback to legacy method
        OpenLegacyCameraProperties(camera.slotIndex);
        return;
    }
    
    wchar_t parameters[64];
    swprintf_s(parameters, 64, L"%d", camera.slotIndex);
    
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
                // Set focus to the name edit control
                SetFocus(GetDlgItem(hDlg, IDC_CAMERA_NAME_EDIT));
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
                            
                            // Try to create the camera
                            if (CreateNewCamera(cameraName)) {
                                EndDialog(hDlg, IDOK);
                                return TRUE;
                            } else {
                                MessageBox(hDlg, "Failed to create camera. The name may already be in use.", "Creation Failed", MB_OK | MB_ICONERROR);
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
                
                return (INT_PTR)TRUE;
            }
            
        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                
                switch (wmId) {
                    case IDC_ADD_NEW_CAMERA:
                        {
                            // Show Add New Camera dialog
                            INT_PTR result = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ADD_CAMERA_DIALOG), hDlg, AddCameraDialogProc);
                            if (result == IDOK) {
                                // Camera was created successfully, refresh the list
                                RefreshCameraList(hListView);
                                LOG("New camera added successfully\n");
                            }
                        }
                        break;
                        
                    case IDC_REGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1 && selected < (int)g_dynamicCameras.size()) {
                                const auto& camera = g_dynamicCameras[selected];
                                if (RegisterCamera(camera)) {
                                    RefreshCameraList(hListView);
                                    LOG("Camera '%s' registered successfully\n", camera.name.c_str());
                                } else {
                                    LOG("Failed to register camera '%s'\n", camera.name.c_str());
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to register.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_UNREGISTER_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1 && selected < (int)g_dynamicCameras.size()) {
                                const auto& camera = g_dynamicCameras[selected];
                                if (UnregisterCamera(camera)) {
                                    RefreshCameraList(hListView);
                                    LOG("Camera '%s' unregistered successfully\n", camera.name.c_str());
                                } else {
                                    LOG("Failed to unregister camera '%s'\n", camera.name.c_str());
                                }
                            } else {
                                MessageBox(hDlg, "Please select a camera to unregister.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        break;
                        
                    case IDC_REMOVE_CAMERA:
                        {
                            int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                            if (selected != -1 && selected < (int)g_dynamicCameras.size()) {
                                const auto& camera = g_dynamicCameras[selected];
                                
                                char confirmMsg[512];
                                sprintf_s(confirmMsg, "Are you sure you want to remove camera '%s'?\n\nThis will permanently delete the camera and all its settings.", camera.name.c_str());
                                
                                int result = MessageBox(hDlg, confirmMsg, "Confirm Removal", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                                if (result == IDYES) {
                                    if (RemoveCamera(camera)) {
                                        RefreshCameraList(hListView);
                                        LOG("Camera '%s' removed successfully\n", camera.name.c_str());
                                    } else {
                                        LOG("Failed to remove camera '%s'\n", camera.name.c_str());
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
                                
                                // Unregister all 8 cameras and delete their configurations
                                for (int i = 0; i < MAX_CAMERAS; i++) {
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
                                
                                // Force refresh to show updated status
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
                            if (selected != -1 && selected < (int)g_dynamicCameras.size()) {
                                const auto& camera = g_dynamicCameras[selected];
                                OpenCameraProperties(camera);
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