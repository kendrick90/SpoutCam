//
// SpoutCamSettingsConsole.cpp - Debug console version
//

#include <windows.h>
#include <iostream>
#include <stdio.h>

// Registry utility function
bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* pValue)
{
    HKEY key;
    if (RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    bool result = (RegQueryValueExA(key, valuename, nullptr, &type, (LPBYTE)pValue, &size) == ERROR_SUCCESS);
    
    RegCloseKey(key);
    return result;
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
    
    printf("Checking registry key: %s\n", keyName);
    
    // Check if any settings exist
    bool hasFps = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &testValue);
    bool hasRes = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &testValue);
    bool hasMirror = ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &testValue);
    
    printf("  fps: %s, resolution: %s, mirror: %s\n", 
           hasFps ? "found" : "not found",
           hasRes ? "found" : "not found", 
           hasMirror ? "found" : "not found");
    
    return hasFps || hasRes || hasMirror;
}

int main()
{
    printf("SpoutCam Settings Debug Console\n");
    printf("==============================\n\n");
    
    printf("Scanning for existing SpoutCam configurations...\n");
    
    for (int i = 0; i < 8; i++) {
        printf("\nSpoutCam%d:\n", i + 1);
        bool hasSettings = HasCameraSettings(i);
        printf("  Status: %s\n", hasSettings ? "Configured" : "Default/Not Found");
    }
    
    printf("\nPress any key to exit...");
    getchar();
    
    return 0;
}