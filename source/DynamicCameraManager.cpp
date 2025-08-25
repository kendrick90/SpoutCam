#include "DynamicCameraManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace SpoutCam {

// Static member initialization
DynamicCameraManager* DynamicCameraManager::s_instance = nullptr;

// Base CLSIDs - these will be modified based on camera name to generate unique CLSIDs
const GUID DynamicCameraManager::BASE_CAMERA_CLSID = 
    {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x00}};
const GUID DynamicCameraManager::BASE_PROPPAGE_CLSID = 
    {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x00}};

DynamicCameraManager* DynamicCameraManager::GetInstance() {
    if (!s_instance) {
        s_instance = new DynamicCameraManager();
        s_instance->LoadCamerasFromRegistry();
    }
    return s_instance;
}

void DynamicCameraManager::Cleanup() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

void DynamicCameraManager::HashStringToGuid(const std::string& str, GUID& guid) {
    // Simple hash function to generate consistent GUIDs from strings
    // This ensures the same camera name always generates the same CLSID
    
    // Start with base GUID
    guid = BASE_CAMERA_CLSID;
    
    // Use simple hash to modify the last 8 bytes
    size_t hash1 = std::hash<std::string>{}(str);
    size_t hash2 = std::hash<std::string>{}(str + "_salt");
    
    // Modify Data4 array with hash values
    guid.Data4[0] = (hash1 >> 0) & 0xFF;
    guid.Data4[1] = (hash1 >> 8) & 0xFF;
    guid.Data4[2] = (hash1 >> 16) & 0xFF;
    guid.Data4[3] = (hash1 >> 24) & 0xFF;
    guid.Data4[4] = (hash2 >> 0) & 0xFF;
    guid.Data4[5] = (hash2 >> 8) & 0xFF;
    guid.Data4[6] = (hash2 >> 16) & 0xFF;
    guid.Data4[7] = (hash2 >> 24) & 0xFF;
    
    // Also modify Data1 for more uniqueness
    guid.Data1 ^= (DWORD)hash1;
}

GUID DynamicCameraManager::GenerateCameraClsid(const std::string& cameraName) {
    GUID guid;
    HashStringToGuid(cameraName, guid);
    return guid;
}

GUID DynamicCameraManager::GeneratePropPageClsid(const std::string& cameraName) {
    GUID guid;
    HashStringToGuid(cameraName + "_PropPage", guid);
    // Ensure it's different from camera CLSID
    guid.Data2 ^= 0xFFFF;
    return guid;
}

DynamicCameraConfig* DynamicCameraManager::CreateCamera(const std::string& cameraName) {
    if (cameraName.empty()) return nullptr;
    
    // Check if camera already exists
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        return &it->second;
    }
    
    // Create new camera configuration
    DynamicCameraConfig config;
    config.name = cameraName;
    config.clsid = GenerateCameraClsid(cameraName);
    config.propPageClsid = GeneratePropPageClsid(cameraName);
    config.isRegistered = false;
    
    // Add to maps
    m_cameras[cameraName] = config;
    m_clsidToName[GuidToString(config.clsid)] = cameraName;
    
    // Save to registry
    SaveCameraToRegistry(config);
    
    return &m_cameras[cameraName];
}

DynamicCameraConfig* DynamicCameraManager::GetCamera(const std::string& cameraName) {
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        return &it->second;
    }
    return nullptr;
}

DynamicCameraConfig* DynamicCameraManager::GetCameraByCLSID(const GUID& clsid) {
    std::string clsidStr = GuidToString(clsid);
    auto it = m_clsidToName.find(clsidStr);
    if (it != m_clsidToName.end()) {
        return GetCamera(it->second);
    }
    return nullptr;
}

bool DynamicCameraManager::DeleteCamera(const std::string& cameraName) {
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        m_clsidToName.erase(GuidToString(it->second.clsid));
        m_cameras.erase(it);
        DeleteCameraFromRegistry(cameraName);
        return true;
    }
    return false;
}

std::vector<DynamicCameraConfig*> DynamicCameraManager::GetAllCameras() {
    std::vector<DynamicCameraConfig*> cameras;
    for (auto& pair : m_cameras) {
        cameras.push_back(&pair.second);
    }
    return cameras;
}

std::string DynamicCameraManager::GuidToString(const GUID& guid) {
    char buffer[40];
    sprintf_s(buffer, sizeof(buffer),
        "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1],
        guid.Data4[2], guid.Data4[3], guid.Data4[4],
        guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buffer);
}

bool DynamicCameraManager::StringToGuid(const std::string& str, GUID& guid) {
    return sscanf_s(str.c_str(),
        "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
        &guid.Data1, &guid.Data2, &guid.Data3,
        &guid.Data4[0], &guid.Data4[1],
        &guid.Data4[2], &guid.Data4[3], &guid.Data4[4],
        &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]) == 11;
}

bool DynamicCameraManager::LoadCamerasFromRegistry() {
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, 
        "Software\\Leading Edge\\SpoutCam\\DynamicCameras", 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        // Create default cameras if registry doesn't exist
        CreateCamera("SpoutCam");
        CreateCamera("SpoutCam2");
        CreateCamera("SpoutCam3");
        return false;
    }
    
    DWORD index = 0;
    char subkeyName[256];
    DWORD subkeyNameSize = sizeof(subkeyName);
    
    while (RegEnumKeyExA(hKey, index++, subkeyName, &subkeyNameSize, 
           nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        
        HKEY hCameraKey;
        std::string cameraPath = "Software\\Leading Edge\\SpoutCam\\DynamicCameras\\";
        cameraPath += subkeyName;
        
        if (RegOpenKeyExA(HKEY_CURRENT_USER, cameraPath.c_str(), 0, KEY_READ, &hCameraKey) == ERROR_SUCCESS) {
            DynamicCameraConfig config;
            config.name = subkeyName;
            
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            DWORD type;
            
            // Read CLSID
            if (RegQueryValueExA(hCameraKey, "CLSID", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                StringToGuid(buffer, config.clsid);
            } else {
                config.clsid = GenerateCameraClsid(config.name);
            }
            
            // Read PropPage CLSID
            bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hCameraKey, "PropPageCLSID", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                StringToGuid(buffer, config.propPageClsid);
            } else {
                config.propPageClsid = GeneratePropPageClsid(config.name);
            }
            
            // Read registration status
            DWORD registered = 0;
            bufferSize = sizeof(DWORD);
            if (RegQueryValueExA(hCameraKey, "Registered", nullptr, &type, (LPBYTE)&registered, &bufferSize) == ERROR_SUCCESS) {
                config.isRegistered = (registered != 0);
            }
            
            // Read sender name
            bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hCameraKey, "SenderName", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                config.senderName = buffer;
            }
            
            m_cameras[config.name] = config;
            m_clsidToName[GuidToString(config.clsid)] = config.name;
            
            RegCloseKey(hCameraKey);
        }
        
        subkeyNameSize = sizeof(subkeyName);
    }
    
    RegCloseKey(hKey);
    
    // Create default cameras if none exist
    if (m_cameras.empty()) {
        CreateCamera("SpoutCam");
        CreateCamera("SpoutCam2");
        CreateCamera("SpoutCam3");
    }
    
    return true;
}

bool DynamicCameraManager::SaveCameraToRegistry(const DynamicCameraConfig& camera) {
    HKEY hKey;
    std::string keyPath = "Software\\Leading Edge\\SpoutCam\\DynamicCameras\\";
    keyPath += camera.name;
    
    LONG result = RegCreateKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    
    if (result != ERROR_SUCCESS) return false;
    
    // Write CLSID
    std::string clsidStr = GuidToString(camera.clsid);
    RegSetValueExA(hKey, "CLSID", 0, REG_SZ, (LPBYTE)clsidStr.c_str(), (DWORD)clsidStr.length() + 1);
    
    // Write PropPage CLSID
    std::string propClsidStr = GuidToString(camera.propPageClsid);
    RegSetValueExA(hKey, "PropPageCLSID", 0, REG_SZ, (LPBYTE)propClsidStr.c_str(), (DWORD)propClsidStr.length() + 1);
    
    // Write registration status
    DWORD registered = camera.isRegistered ? 1 : 0;
    RegSetValueExA(hKey, "Registered", 0, REG_DWORD, (LPBYTE)&registered, sizeof(DWORD));
    
    // Write sender name
    if (!camera.senderName.empty()) {
        RegSetValueExA(hKey, "SenderName", 0, REG_SZ, 
            (LPBYTE)camera.senderName.c_str(), (DWORD)camera.senderName.length() + 1);
    }
    
    RegCloseKey(hKey);
    return true;
}

bool DynamicCameraManager::DeleteCameraFromRegistry(const std::string& cameraName) {
    std::string keyPath = "Software\\Leading Edge\\SpoutCam\\DynamicCameras\\";
    keyPath += cameraName;
    
    LONG result = RegDeleteKeyA(HKEY_CURRENT_USER, keyPath.c_str());
    return result == ERROR_SUCCESS;
}

bool DynamicCameraManager::IsCameraRegistered(const std::string& cameraName) {
    auto camera = GetCamera(cameraName);
    return camera ? camera->isRegistered : false;
}

void DynamicCameraManager::SetCameraRegistered(const std::string& cameraName, bool registered) {
    auto camera = GetCamera(cameraName);
    if (camera) {
        camera->isRegistered = registered;
        SaveCameraToRegistry(*camera);
    }
}

std::string DynamicCameraManager::GenerateAvailableName(const std::string& baseName) {
    if (m_cameras.find(baseName) == m_cameras.end()) {
        return baseName;
    }
    
    int counter = 2;
    std::string newName;
    do {
        newName = baseName + std::to_string(counter++);
    } while (m_cameras.find(newName) != m_cameras.end());
    
    return newName;
}

// C-style export functions for compatibility

extern "C" __declspec(dllexport) 
bool GetDynamicCameraConfig(int index, void* configOut) {
    if (!configOut) return false;
    
    auto cameras = DynamicCameraManager::GetInstance()->GetAllCameras();
    if (index < 0 || index >= (int)cameras.size()) return false;
    
    DynamicCameraConfig* config = cameras[index];
    memcpy(configOut, config, sizeof(DynamicCameraConfig));
    return true;
}

extern "C" __declspec(dllexport) 
bool GetDynamicCameraConfigByName(const char* name, void* configOut) {
    if (!name || !configOut) return false;
    
    auto config = DynamicCameraManager::GetInstance()->GetCamera(name);
    if (!config) return false;
    
    memcpy(configOut, config, sizeof(DynamicCameraConfig));
    return true;
}

extern "C" __declspec(dllexport) 
bool CreateDynamicCamera(const char* name) {
    if (!name) return false;
    return DynamicCameraManager::GetInstance()->CreateCamera(name) != nullptr;
}

extern "C" __declspec(dllexport) 
bool DeleteDynamicCamera(const char* name) {
    if (!name) return false;
    return DynamicCameraManager::GetInstance()->DeleteCamera(name);
}

extern "C" __declspec(dllexport) 
int GetDynamicCameraCount() {
    return (int)DynamicCameraManager::GetInstance()->GetAllCameras().size();
}

extern "C" __declspec(dllexport) 
bool GetDynamicCameraName(int index, char* nameOut, int bufferSize) {
    if (!nameOut || bufferSize <= 0) return false;
    
    auto cameras = DynamicCameraManager::GetInstance()->GetAllCameras();
    if (index < 0 || index >= (int)cameras.size()) return false;
    
    strncpy_s(nameOut, bufferSize, cameras[index]->name.c_str(), _TRUNCATE);
    return true;
}

} // namespace SpoutCam