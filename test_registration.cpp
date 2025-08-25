#include <windows.h>
#include <iostream>
#include <string>

// Simple test program to verify CLSID registration/lookup
int main() {
    std::cout << "=== SpoutCam Registration Test ===" << std::endl;
    
    // Test 1: Check if any SpoutCam CLSIDs are registered
    std::cout << "\n1. Scanning for registered SpoutCam CLSIDs..." << std::endl;
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char clsidName[256];
        DWORD clsidNameSize;
        
        while (true) {
            clsidNameSize = sizeof(clsidName);
            if (RegEnumKeyExA(hKey, index, clsidName, &clsidNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            
            // Check if this CLSID has a SpoutCam-related description
            HKEY hClsidKey;
            std::string clsidPath = std::string("CLSID\\") + clsidName;
            if (RegOpenKeyExA(HKEY_CLASSES_ROOT, clsidPath.c_str(), 0, KEY_READ, &hClsidKey) == ERROR_SUCCESS) {
                char description[256] = {0};
                DWORD descSize = sizeof(description);
                
                if (RegQueryValueExA(hClsidKey, NULL, NULL, NULL, (BYTE*)description, &descSize) == ERROR_SUCCESS) {
                    std::string desc(description);
                    if (desc.find("SpoutCam") != std::string::npos || desc.find("Settings") != std::string::npos) {
                        std::cout << "   Found: " << clsidName << " = \"" << description << "\"" << std::endl;
                    }
                }
                RegCloseKey(hClsidKey);
            }
            
            index++;
        }
        RegCloseKey(hKey);
    }
    
    // Test 2: Check DirectShow filter registration
    std::cout << "\n2. Checking DirectShow filter registration..." << std::endl;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char instanceName[256];
        DWORD instanceNameSize;
        
        while (true) {
            instanceNameSize = sizeof(instanceName);
            if (RegEnumKeyExA(hKey, index, instanceName, &instanceNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            
            // Check FriendlyName
            HKEY hInstanceKey;
            if (RegOpenKeyExA(hKey, instanceName, 0, KEY_READ, &hInstanceKey) == ERROR_SUCCESS) {
                char friendlyName[256] = {0};
                DWORD nameSize = sizeof(friendlyName);
                
                if (RegQueryValueExA(hInstanceKey, "FriendlyName", NULL, NULL, (BYTE*)friendlyName, &nameSize) == ERROR_SUCCESS) {
                    std::string name(friendlyName);
                    if (name.find("SpoutCam") != std::string::npos) {
                        std::cout << "   DirectShow Filter: " << instanceName << " = \"" << friendlyName << "\"" << std::endl;
                    }
                }
                RegCloseKey(hInstanceKey);
            }
            
            index++;
        }
        RegCloseKey(hKey);
    }
    
    // Test 3: Try to load and test the SpoutCam DLL functions
    std::cout << "\n3. Testing DLL functions..." << std::endl;
    
    HMODULE hDll = LoadLibraryA("binaries\\SPOUTCAM\\SpoutCam64\\SpoutCam64.ax");
    if (hDll) {
        std::cout << "   ✓ DLL loaded successfully" << std::endl;
        
        // Test RegisterCameraByName
        typedef HRESULT (STDAPICALLTYPE *RegisterCameraByNameFunc)(const char*);
        RegisterCameraByNameFunc registerFunc = (RegisterCameraByNameFunc)GetProcAddress(hDll, "RegisterCameraByName");
        
        if (registerFunc) {
            std::cout << "   ✓ RegisterCameraByName function found" << std::endl;
            
            // Test registering a camera
            HRESULT hr = registerFunc("TestCamera");
            std::cout << "   RegisterCameraByName('TestCamera') returned: 0x" << std::hex << hr << std::endl;
            
            // Test unregistering
            typedef HRESULT (STDAPICALLTYPE *UnregisterCameraByNameFunc)(const char*);
            UnregisterCameraByNameFunc unregisterFunc = (UnregisterCameraByNameFunc)GetProcAddress(hDll, "UnregisterCameraByName");
            
            if (unregisterFunc) {
                std::cout << "   ✓ UnregisterCameraByName function found" << std::endl;
                hr = unregisterFunc("TestCamera");
                std::cout << "   UnregisterCameraByName('TestCamera') returned: 0x" << std::hex << hr << std::endl;
            } else {
                std::cout << "   ✗ UnregisterCameraByName function not found" << std::endl;
            }
        } else {
            std::cout << "   ✗ RegisterCameraByName function not found" << std::endl;
        }
        
        FreeLibrary(hDll);
    } else {
        std::cout << "   ✗ Failed to load DLL (error: " << GetLastError() << ")" << std::endl;
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}