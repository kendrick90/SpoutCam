#include "DynamicCameraManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace SpoutCam {

// Static member initialization
DynamicCameraManager* DynamicCameraManager::s_instance = nullptr;

// Base CLSIDs - these will be modified based on camera name to generate unique CLSIDs
const GUID DynamicCameraManager::BASE_CAMERA_CLSID = 
    {0xEF14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x00}};
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
    // Generate CLSIDs following original SpoutCam pattern for consistent DirectShow sorting
    // Original pattern: increment only the last byte like {8E14549A-...-E933}, {8E14549A-...-E934}
    
    // Start with original base GUID (matches upstream/master)
    guid = {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33}};
    
    // Use Data2, Data3, and Data4 for better uniqueness (12 bytes total = 96 bits)
    // Format: 8E14549A-XXXX-YYYY-AFA1-3578E927ZZZZ
    
    // Create hash from camera name
    uint64_t hash = 0;
    for (size_t i = 0; i < str.length(); i++) {
        hash = hash * 31 + (uint8_t)str[i];
    }
    hash ^= (uint64_t)str.length() << 56;  // Include length
    
    // Use Data2 (16 bits)
    guid.Data2 = (uint16_t)(hash & 0xFFFF);
    
    // Use Data3 (16 bits) 
    guid.Data3 = (uint16_t)((hash >> 16) & 0xFFFF);
    
    // Use last 4 bytes of Data4 for remaining hash bits
    guid.Data4[4] = (uint8_t)((hash >> 32) & 0xFF);
    guid.Data4[5] = (uint8_t)((hash >> 40) & 0xFF);
    guid.Data4[6] = (uint8_t)((hash >> 48) & 0xFF);
    guid.Data4[7] = (uint8_t)((hash >> 56) & 0xFF);
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
    if (cameraName.empty()) {
        // Never allow empty camera names
        return nullptr;
    }
    
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
    config.isActive = false;
    config.hasSettings = false;
    
    // Add to maps
    m_cameras[cameraName] = config;
    m_clsidToName[GuidToString(config.clsid)] = cameraName;
    m_propPageClsidToName[GuidToString(config.propPageClsid)] = cameraName;
    
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
    // Ensure cameras are loaded from registry first
    if (m_clsidToName.empty()) {
        LoadCamerasFromRegistry();
    }
    
    std::string clsidStr = GuidToString(clsid);
    auto it = m_clsidToName.find(clsidStr);
    if (it != m_clsidToName.end()) {
        return GetCamera(it->second);
    }
    return nullptr;
}

DynamicCameraConfig* DynamicCameraManager::GetCameraByPropPageCLSID(const GUID& propPageClsid) {
    std::string clsidStr = GuidToString(propPageClsid);
    auto it = m_propPageClsidToName.find(clsidStr);
    if (it != m_propPageClsidToName.end()) {
        return GetCamera(it->second);
    }
    return nullptr;
}

bool DynamicCameraManager::DeleteCamera(const std::string& cameraName) {
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        // Remove from manager maps first
        m_clsidToName.erase(GuidToString(it->second.clsid));
        m_propPageClsidToName.erase(GuidToString(it->second.propPageClsid));
        m_cameras.erase(it);
        
        // Delete ALL registry data for this camera
        DeleteCameraFromRegistry(cameraName);
        
        // Note: CLSID unregistration is handled by UnregisterCameraByName() in SpoutCamSettings
        // which calls the DLL's UnregisterCameraByName function before calling DeleteCamera
        
        return true;
    }
    return false;
}

bool DynamicCameraManager::UpdateCameraName(const std::string& oldName, const std::string& newName) {
    auto it = m_cameras.find(oldName);
    if (it != m_cameras.end()) {
        // Get the camera config
        DynamicCameraConfig config = it->second;
        
        // Update internal mappings - remove old entries
        m_clsidToName.erase(GuidToString(config.clsid));
        m_propPageClsidToName.erase(GuidToString(config.propPageClsid));
        m_cameras.erase(it);
        
        // Update camera name
        config.name = newName;
        
        // Add back with new name
        m_cameras[newName] = config;
        m_clsidToName[GuidToString(config.clsid)] = newName;
        m_propPageClsidToName[GuidToString(config.propPageClsid)] = newName;
        
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
    // Remove performance cache that blocks real-time updates
    // Always reload to ensure UI changes are immediately reflected
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, 
        "Software\\Leading Edge\\SpoutCam", 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        // Don't auto-create cameras - let the UI handle camera creation
        return false;
    }
    
    DWORD index = 0;
    char subkeyName[256];
    DWORD subkeyNameSize = sizeof(subkeyName);
    
    while (RegEnumKeyExA(hKey, index++, subkeyName, &subkeyNameSize, 
           nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        
        HKEY hCameraKey;
        // Skip global settings - only process camera subkeys
        if (strcmp(subkeyName, "DynamicCameras") == 0) {
            subkeyNameSize = sizeof(subkeyName);
            continue;
        }
        
        std::string cameraPath = "Software\\Leading Edge\\SpoutCam\\";
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
            
            // Read activation status (try new field name first, fall back to old)
            DWORD active = 0;
            bufferSize = sizeof(DWORD);
            if (RegQueryValueExA(hCameraKey, "Active", nullptr, &type, (LPBYTE)&active, &bufferSize) == ERROR_SUCCESS) {
                config.isActive = (active != 0);
            } else if (RegQueryValueExA(hCameraKey, "Registered", nullptr, &type, (LPBYTE)&active, &bufferSize) == ERROR_SUCCESS) {
                // Backward compatibility
                config.isActive = (active != 0);
            }
            
            // Read settings status
            DWORD hasSettings = 0;
            bufferSize = sizeof(DWORD);
            if (RegQueryValueExA(hCameraKey, "HasSettings", nullptr, &type, (LPBYTE)&hasSettings, &bufferSize) == ERROR_SUCCESS) {
                config.hasSettings = (hasSettings != 0);
            }
            
            // Read sender name
            bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hCameraKey, "SenderName", nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                config.senderName = buffer;
            }
            
            m_cameras[config.name] = config;
            m_clsidToName[GuidToString(config.clsid)] = config.name;
            m_propPageClsidToName[GuidToString(config.propPageClsid)] = config.name;
            
            RegCloseKey(hCameraKey);
        }
        
        subkeyNameSize = sizeof(subkeyName);
    }
    
    RegCloseKey(hKey);
    
    // Clean up any cameras with invalid names that may have been loaded
    std::vector<std::string> invalidCameras;
    for (auto& pair : m_cameras) {
        const std::string& name = pair.first;
        // Check for invalid camera names
        if (name.empty() || 
            name == "Cameras" ||  // Ghost entry that appears in UI
            !IsValidCameraName(name)) {
            invalidCameras.push_back(name);
        }
    }
    
    // Remove all invalid cameras
    for (const std::string& name : invalidCameras) {
        auto it = m_cameras.find(name);
        if (it != m_cameras.end()) {
            m_clsidToName.erase(GuidToString(it->second.clsid));
            m_propPageClsidToName.erase(GuidToString(it->second.propPageClsid));
            m_cameras.erase(it);
            DeleteCameraFromRegistry(name);  // Clean up from registry too
        }
    }
    
    // Don't auto-create default cameras here
    // They are only created when registry doesn't exist (handled above)
    
    return true;
}

bool DynamicCameraManager::SaveCameraToRegistry(const DynamicCameraConfig& camera) {
    HKEY hKey;
    std::string keyPath = "Software\\Leading Edge\\SpoutCam\\";
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
    
    // Write activation status
    DWORD active = camera.isActive ? 1 : 0;
    RegSetValueExA(hKey, "Active", 0, REG_DWORD, (LPBYTE)&active, sizeof(DWORD));
    
    // Write settings status
    DWORD hasSettings = camera.hasSettings ? 1 : 0;
    RegSetValueExA(hKey, "HasSettings", 0, REG_DWORD, (LPBYTE)&hasSettings, sizeof(DWORD));
    
    // Write sender name
    if (!camera.senderName.empty()) {
        RegSetValueExA(hKey, "SenderName", 0, REG_SZ, 
            (LPBYTE)camera.senderName.c_str(), (DWORD)camera.senderName.length() + 1);
    }
    
    RegCloseKey(hKey);
    return true;
}

bool DynamicCameraManager::DeleteCameraFromRegistry(const std::string& cameraName) {
    // Handle corrupted cameras with empty names by enumerating and finding them
    if (cameraName.empty()) {
        // For empty camera names, we need to enumerate all keys and find/delete empty ones
        HKEY hKey;
        LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, 
            "Software\\Leading Edge\\SpoutCam", 0, KEY_ENUMERATE_SUB_KEYS | KEY_WRITE, &hKey);
        
        if (result != ERROR_SUCCESS) {
            return false;  // Registry key doesn't exist
        }
        
        // Look for empty subkey names (corrupted entries)
        DWORD index = 0;
        char subkeyName[256];
        DWORD subkeyNameSize = sizeof(subkeyName);
        bool foundEmptyKey = false;
        
        while (RegEnumKeyExA(hKey, index, subkeyName, &subkeyNameSize, 
               nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            
            if (strlen(subkeyName) == 0) {
                // Found an empty key name - delete it
                LONG deleteResult = RegDeleteKeyA(hKey, subkeyName);
                if (deleteResult == ERROR_SUCCESS) {
                    foundEmptyKey = true;
                }
                break;
            }
            
            index++;
            subkeyNameSize = sizeof(subkeyName);
        }
        
        RegCloseKey(hKey);
        return foundEmptyKey;
    }
    
    // Normal case: delete specific camera registry key
    std::string keyPath = "Software\\Leading Edge\\SpoutCam\\";
    keyPath += cameraName;
    
    LONG result = RegDeleteKeyA(HKEY_CURRENT_USER, keyPath.c_str());
    return result == ERROR_SUCCESS;
}


bool DynamicCameraManager::IsCameraActive(const std::string& cameraName) {
    DynamicCameraConfig* camera = GetCamera(cameraName);
    return camera ? camera->isActive : false;
}

void DynamicCameraManager::SetCameraActive(const std::string& cameraName, bool active) {
    DynamicCameraConfig* camera = GetCamera(cameraName);
    if (camera) {
        camera->isActive = active;
        SaveCameraToRegistry(*camera);
    }
}

bool DynamicCameraManager::CameraHasSettings(const std::string& cameraName) {
    DynamicCameraConfig* camera = GetCamera(cameraName);
    return camera ? camera->hasSettings : false;
}

void DynamicCameraManager::SetCameraHasSettings(const std::string& cameraName, bool hasSettings) {
    DynamicCameraConfig* camera = GetCamera(cameraName);
    if (camera) {
        camera->hasSettings = hasSettings;
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

// Enhanced name-based API implementation

std::vector<std::string> DynamicCameraManager::GetCameraNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_cameras) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> DynamicCameraManager::GetActiveCameraNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_cameras) {
        if (pair.second.isActive) {
            names.push_back(pair.first);
        }
    }
    return names;
}

bool DynamicCameraManager::ValidateCameraName(const std::string& name) const {
    return IsValidCameraName(name) && !CameraExists(name);
}

bool DynamicCameraManager::CameraExists(const std::string& cameraName) const {
    return m_cameras.find(cameraName) != m_cameras.end();
}

bool DynamicCameraManager::ActivateCamera(const std::string& cameraName) {
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        it->second.isActive = true;
        return true;
    }
    return false;
}

bool DynamicCameraManager::DeactivateCamera(const std::string& cameraName) {
    auto it = m_cameras.find(cameraName);
    if (it != m_cameras.end()) {
        it->second.isActive = false;
        return true;
    }
    return false;
}

size_t DynamicCameraManager::GetActiveCameraCount() const {
    size_t count = 0;
    for (const auto& pair : m_cameras) {
        if (pair.second.isActive) {
            count++;
        }
    }
    return count;
}

size_t DynamicCameraManager::GetTotalCameraCount() const {
    return m_cameras.size();
}

std::string DynamicCameraManager::SanitizeCameraName(const std::string& name) const {
    std::string sanitized = name;
    
    // Remove leading/trailing whitespace
    size_t start = sanitized.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        sanitized = sanitized.substr(start);
    }
    size_t end = sanitized.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) {
        sanitized = sanitized.substr(0, end + 1);
    }
    
    // Replace invalid characters with underscores
    for (char& c : sanitized) {
        if (!std::isalnum(c) && c != ' ' && c != '-' && c != '_') {
            c = '_';
        }
    }
    
    // Limit length
    if (sanitized.length() > 64) {
        sanitized = sanitized.substr(0, 64);
    }
    
    // Ensure not empty
    if (sanitized.empty()) {
        sanitized = "SpoutCam";
    }
    
    return sanitized;
}

bool DynamicCameraManager::IsValidCameraName(const std::string& name) const {
    // Check length
    if (name.empty() || name.length() > 64) {
        return false;
    }
    
    // Check for invalid characters
    for (char c : name) {
        if (!std::isalnum(c) && c != ' ' && c != '-' && c != '_') {
            return false;
        }
    }
    
    // Check for leading/trailing whitespace
    if (name.front() == ' ' || name.back() == ' ') {
        return false;
    }
    
    return true;
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