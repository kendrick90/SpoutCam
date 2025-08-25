#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

// Dynamic Camera Management System
// This replaces the fixed slot-based system with a fully dynamic approach
// where CLSIDs are generated based on camera names

namespace SpoutCam {

// Structure to hold dynamic camera configuration
struct DynamicCameraConfig {
    std::string name;
    GUID clsid;
    GUID propPageClsid;
    bool isRegistered;
    std::string senderName;  // Associated Spout sender
    
    DynamicCameraConfig() : isRegistered(false) {
        memset(&clsid, 0, sizeof(GUID));
        memset(&propPageClsid, 0, sizeof(GUID));
    }
};

class DynamicCameraManager {
private:
    static DynamicCameraManager* s_instance;
    std::map<std::string, DynamicCameraConfig> m_cameras;
    std::map<std::string, std::string> m_clsidToName; // GUID string -> camera name (for camera CLSIDs)
    std::map<std::string, std::string> m_propPageClsidToName; // GUID string -> camera name (for prop page CLSIDs)
    
    // Base CLSIDs for generation
    static const GUID BASE_CAMERA_CLSID;
    static const GUID BASE_PROPPAGE_CLSID;
    
    DynamicCameraManager() {}
    
public:
    static DynamicCameraManager* GetInstance();
    static void Cleanup();
    
    // Generate CLSIDs based on camera name using hash
    GUID GenerateCameraClsid(const std::string& cameraName);
    GUID GeneratePropPageClsid(const std::string& cameraName);
    
    // Camera management
    DynamicCameraConfig* CreateCamera(const std::string& cameraName);
    DynamicCameraConfig* GetCamera(const std::string& cameraName);
    DynamicCameraConfig* GetCameraByCLSID(const GUID& clsid);
    DynamicCameraConfig* GetCameraByPropPageCLSID(const GUID& propPageClsid);
    bool DeleteCamera(const std::string& cameraName);
    
    // Get all cameras
    std::vector<DynamicCameraConfig*> GetAllCameras();
    
    // Registry operations
    bool LoadCamerasFromRegistry();
    bool SaveCameraToRegistry(const DynamicCameraConfig& camera);
    bool DeleteCameraFromRegistry(const std::string& cameraName);
    
    // Registration status
    bool IsCameraRegistered(const std::string& cameraName);
    void SetCameraRegistered(const std::string& cameraName, bool registered);
    
    // Find available name
    std::string GenerateAvailableName(const std::string& baseName = "SpoutCam");
    
private:
    // Hash function for GUID generation
    void HashStringToGuid(const std::string& str, GUID& guid);
    std::string GuidToString(const GUID& guid);
    bool StringToGuid(const std::string& str, GUID& guid);
};

// Helper functions for compatibility with existing code
extern "C" {
    // Get camera configuration by index (for backward compatibility)
    __declspec(dllexport) bool GetDynamicCameraConfig(int index, void* configOut);
    
    // Get camera configuration by name
    __declspec(dllexport) bool GetDynamicCameraConfigByName(const char* name, void* configOut);
    
    // Create new camera
    __declspec(dllexport) bool CreateDynamicCamera(const char* name);
    
    // Delete camera
    __declspec(dllexport) bool DeleteDynamicCamera(const char* name);
    
    // Get camera count
    __declspec(dllexport) int GetDynamicCameraCount();
    
    // Get camera name by index
    __declspec(dllexport) bool GetDynamicCameraName(int index, char* nameOut, int bufferSize);
}

} // namespace SpoutCam