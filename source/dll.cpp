//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Spout Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")

#include <stdio.h>
#include <conio.h>
#include "cam.h"
#include "DynamicCameraManager.h"
#include <vector>
#include <string>

//<==================== VS-START ====================>
#ifdef DIALOG_WITHOUT_REGISTRATION
#pragma comment(lib, "Comctl32.lib")
#endif

#include "camprops.h"
//<==================== VS-END ======================>

#include <olectl.h>
#include <initguid.h>
#include <dllsetup.h>

// Include DirectShow base classes for CClassFactory
#include <streams.h>

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer( CLSID clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );
HRESULT RegisterSingleCameraFilter( BOOL bRegister, int cameraIndex );
HRESULT RegisterCameraInstanceOnly(const GUID& instanceGuid, const WCHAR* friendlyName);
HRESULT UnregisterCameraInstanceOnly(const GUID& instanceGuid);
HRESULT RegisterPrimaryCameraFilter( BOOL bRegister );

// Primary SpoutCam CLSIDs - only these get registered in the system
const GUID CLSID_SpoutCam_Primary = {0x8e14549a, 0xdb61, 0x4309, {0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33}};
const GUID CLSID_SpoutCam_PropPage_Primary = {0xcd7780b7, 0x40d2, 0x4f33, {0x80, 0xe2, 0xb0, 0x2e, 0x00, 0x9c, 0xe6, 0x33}};

//
// The NAME OF THE CAMERA CAN BE CHANGED HERE
//<==================== VS-START ====================>
// Actual name now defined as LPCTSTR literal makro in cam.h.
// This was needed because the NAME() makros in debug mode need a LPCTSTR,
// and therefor the debug version previously didn't compile.
const WCHAR SpoutCamName[] = L"" SPOUTCAMNAME;
//<==================== VS-END ======================>

//
// DYNAMIC CAMERA SUPPORT - UNLIMITED CAMERAS
//
// The system now dynamically generates CLSIDs based on camera names.
// No longer limited to a fixed number of cameras.
//
// Dynamic camera configurations vector
std::vector<SpoutCamConfig> g_DynamicCameraConfigs;

// Keep the original CLSID for backward compatibility
DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33);

//<==================== VS-START ====================>
// And the property page we support

// {CD7780B7-40D2-4F33-80E2-B02E009CE63F}
DEFINE_GUID(CLSID_SpoutCamPropertyPage,
	0xcd7780b7, 0x40d2, 0x4f33, 0x80, 0xe2, 0xb0, 0x2e, 0x0, 0x9c, 0xe6, 0x3f);

// {6CD0D97B-C242-48DA-916D-28856D00754B}
DEFINE_GUID(IID_ICamSettings,
	0x6cd0d97b, 0xc242, 0x48da, 0x91, 0x6d, 0x28, 0x85, 0x6d, 0x00, 0x75, 0x4b);
//<==================== VS-END ======================>

const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam = 
{ 
    &MEDIATYPE_Video, 
    &MEDIASUBTYPE_NULL 
};

const AMOVIESETUP_PIN AMSPinVCam=
{
    L"Output",             // Pin string name
    FALSE,                 // Is it rendered
    TRUE,                  // Is it an output
    FALSE,                 // Can we have none
    FALSE,                 // Can we have many
    &CLSID_NULL,           // Connects to filter - obsolete
    NULL,                  // Connects to pin - obsolete
    1,                     // Number of types
    &AMSMediaTypesVCam     // Pointer to media types
};

// Static filter setup for single CLSID approach
const AMOVIESETUP_FILTER AMSFilterVCam = 
{
    &CLSID_SpoutCam_Primary,  // Filter CLSID
    L"SpoutCam",              // String name
    MERIT_DO_NOT_USE,         // Filter merit
    1,                        // Number pins
    &AMSPinVCam               // Pin details
};

// Dynamic filter setup arrays
std::vector<AMOVIESETUP_FILTER> AMSFilters;
std::vector<std::wstring> CameraNames;

// Registry helper functions - now handled by DynamicCameraManager

// Initialize filter setups from dynamic camera manager
void InitializeFilterSetups() {
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto cameras = manager->GetAllCameras();
    
    // Clear existing
    g_DynamicCameraConfigs.clear();
    AMSFilters.clear();
    CameraNames.clear();
    
    // Create configs for each dynamic camera
    for (auto camera : cameras) {
        SpoutCamConfig config;
        config.name = _strdup(camera->name.c_str());
        config.clsid = camera->clsid;
        config.propPageClsid = camera->propPageClsid;
        g_DynamicCameraConfigs.push_back(config);
        
        // Convert camera name to wide string
        WCHAR wideName[256];
        MultiByteToWideChar(CP_ACP, 0, camera->name.c_str(), -1, wideName, 256);
        CameraNames.push_back(std::wstring(wideName));
        
        // Create filter setup
        AMOVIESETUP_FILTER filter;
        filter.clsID = &g_DynamicCameraConfigs.back().clsid;
        filter.strName = CameraNames.back().c_str();
        filter.dwMerit = MERIT_DO_NOT_USE;
        filter.nPins = 1;
        filter.lpPin = &AMSPinVCam;
        AMSFilters.push_back(filter);
    }
}

// Dynamic camera factory function
CUnknown * WINAPI CreateDynamicCamera(LPUNKNOWN lpunk, HRESULT *phr, const GUID& clsid) {
    return CVCam::CreateCameraInstance(lpunk, phr, clsid);
}

// Maximum supported dynamic cameras
#define MAX_DYNAMIC_CAMERAS 256

// Static array for DirectShow compatibility
CFactoryTemplate g_Templates[MAX_DYNAMIC_CAMERAS * 2];
int g_cTemplates = 0;

// CLSID comparator for std::map
struct CLSIDComparator {
    bool operator()(const CLSID& lhs, const CLSID& rhs) const {
        return memcmp(&lhs, &rhs, sizeof(CLSID)) < 0;
    }
};

// CLSID to camera name mapping for factory functions
std::map<CLSID, std::string, CLSIDComparator> g_CLSIDToCameraName;

// Property page CLSID to camera name mapping
std::map<CLSID, std::string, CLSIDComparator> g_PropPageCLSIDToCameraName;

// Predefined property page factory functions for different cameras
CUnknown * WINAPI CreatePropertyPageInstance0(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance1(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance2(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance3(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance4(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance5(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance6(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreatePropertyPageInstance7(LPUNKNOWN lpunk, HRESULT *phr);

// Array of factory functions
CUnknown * (WINAPI *g_PropertyPageFactoryFunctions[])(LPUNKNOWN, HRESULT *) = {
    CreatePropertyPageInstance0, CreatePropertyPageInstance1, CreatePropertyPageInstance2, CreatePropertyPageInstance3,
    CreatePropertyPageInstance4, CreatePropertyPageInstance5, CreatePropertyPageInstance6, CreatePropertyPageInstance7
};

// Camera names for each factory function index
std::vector<std::string> g_FactoryIndexToCameraName;

// Individual camera factory functions - similar to property pages
CUnknown * WINAPI CreateCameraInstance0(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance1(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance2(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance3(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance4(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance5(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance6(LPUNKNOWN lpunk, HRESULT *phr);
CUnknown * WINAPI CreateCameraInstance7(LPUNKNOWN lpunk, HRESULT *phr);

// Array of camera factory functions
CUnknown * (WINAPI *g_CameraFactoryFunctions[])(LPUNKNOWN, HRESULT *) = {
    CreateCameraInstance0, CreateCameraInstance1, CreateCameraInstance2, CreateCameraInstance3,
    CreateCameraInstance4, CreateCameraInstance5, CreateCameraInstance6, CreateCameraInstance7
};



// Enhanced property page factory that determines camera from CLSID mapping
CUnknown * WINAPI CreatePropertyPageInstance(LPUNKNOWN lpunk, HRESULT *phr) {
    // Enable console for debugging
    static bool consoleAllocated = false;
    if (!consoleAllocated) {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
        SetConsoleTitle(L"SpoutCam Property Page Debug");
        consoleAllocated = true;
    }
    
    // Also write to log file
    FILE* logFile = nullptr;
    fopen_s(&logFile, "C:\\temp\\spoutcam_debug.log", "a");
    
    auto logOutput = [&](const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        if (logFile) {
            vfprintf(logFile, format, args);
            fflush(logFile);
        }
        va_end(args);
    };
    
    logOutput("\n=== CreatePropertyPageInstance CALLED ===\n");
    logOutput("lpunk = %p\n", lpunk);
    
    // Try to determine which camera this property page is for
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    auto cameras = manager->GetAllCameras();
    
    logOutput("Available cameras: %d\n", (int)cameras.size());
    for (size_t i = 0; i < cameras.size(); i++) {
        logOutput("  Camera %d: '%s'\n", (int)i, cameras[i]->name.c_str());
    }
    
    // If there's only one camera, use it
    if (cameras.size() == 1) {
        std::string cameraName = cameras[0]->name;
        logOutput("SUCCESS: Only one camera available, using '%s'\n", cameraName.c_str());
        if (logFile) fclose(logFile);
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, cameraName);
    }
    
    // If multiple cameras exist, we need a way to determine which one
    // Check if we can find it in the CLSID mapping
    logOutput("Multiple cameras available, checking CLSID mappings...\n");
    if (!g_PropPageCLSIDToCameraName.empty()) {
        // For now, use the first mapped camera as a fallback
        std::string cameraName = g_PropPageCLSIDToCameraName.begin()->second;
        logOutput("Using first mapped camera: '%s'\n", cameraName.c_str());
        if (logFile) fclose(logFile);
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, cameraName);
    }
    
    // Try to extract camera info from the filter that owns this property page
    if (lpunk) {
        logOutput("Attempting to query lpunk for ICamSettings interface...\n");
        
        // Query for ICamSettings interface which should give us camera info
        ICamSettings* pCamSettings = nullptr;
        HRESULT hr = lpunk->QueryInterface(IID_ICamSettings, (void**)&pCamSettings);
        if (SUCCEEDED(hr) && pCamSettings) {
            logOutput("SUCCESS: Got ICamSettings interface from filter\n");
            
            // Try to get camera name from the filter
            char cameraName[256] = {0};
            hr = pCamSettings->get_CameraName(cameraName, sizeof(cameraName));
            if (SUCCEEDED(hr) && strlen(cameraName) > 0) {
                logOutput("SUCCESS: Got camera name from filter: '%s'\n", cameraName);
                pCamSettings->Release();
                if (logFile) fclose(logFile);
                return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, std::string(cameraName));
            }
            
            // Try to get camera index and derive name
            int cameraIndex = 0;
            hr = pCamSettings->get_CameraIndex(&cameraIndex);
            if (SUCCEEDED(hr)) {
                logOutput("Got camera index from filter: %d\n", cameraIndex);
                // Convert index to camera name using manager
                auto cameras = manager->GetAllCameras();
                if (cameraIndex >= 0 && cameraIndex < (int)cameras.size()) {
                    std::string cameraName = cameras[cameraIndex]->name;
                    logOutput("SUCCESS: Resolved camera name from index: '%s'\n", cameraName.c_str());
                    pCamSettings->Release();
                    if (logFile) fclose(logFile);
                    return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, cameraName);
                }
            }
            
            pCamSettings->Release();
        } else {
            logOutput("Failed to get ICamSettings interface, hr = 0x%08X\n", hr);
        }
        
        if (SUCCEEDED(hr) && pCamSettings) {
            logOutput("SUCCESS: Got ICamSettings interface\n");
            // Try to get camera name from the filter
            char cameraName[256] = {0};
            HRESULT nameResult = pCamSettings->get_CameraName(cameraName, 256);
            logOutput("get_CameraName result: 0x%08X, name: '%s'\n", nameResult, cameraName);
            
            if (SUCCEEDED(nameResult) && strlen(cameraName) > 0) {
                logOutput("SUCCESS: Creating property page for camera '%s' using filter interface\n", cameraName);
                pCamSettings->Release();
                if (logFile) fclose(logFile);
                return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, std::string(cameraName));
            } else {
                logOutput("WARNING: get_CameraName failed or returned empty name\n");
            }
            pCamSettings->Release();
        } else {
            logOutput("WARNING: Failed to get ICamSettings interface\n");
        }
    } else {
        logOutput("WARNING: lpunk is NULL\n");
    }
    
    // Final fallback: create default property page
    logOutput("FALLBACK: Creating default property page (camera detection failed)\n");
    logOutput("=== CreatePropertyPageInstance END ===\n\n");
    logOutput("Press any key in this console to continue debugging...\n");
    
    if (logFile) fclose(logFile);
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

// Individual property page factory functions - each knows which camera it represents
CUnknown * WINAPI CreatePropertyPageInstance0(LPUNKNOWN lpunk, HRESULT *phr) {
    if (phr) *phr = S_OK;
    
    try {
        if (0 < g_FactoryIndexToCameraName.size()) {
            std::string cameraName = g_FactoryIndexToCameraName[0];
            if (!cameraName.empty()) {
                return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, cameraName);
            }
        }
    } catch (...) {
        if (phr) *phr = E_FAIL;
        return nullptr;
    }
    
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance1(LPUNKNOWN lpunk, HRESULT *phr) {
    if (1 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[1]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance2(LPUNKNOWN lpunk, HRESULT *phr) {
    if (2 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[2]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance3(LPUNKNOWN lpunk, HRESULT *phr) {
    if (3 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[3]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance4(LPUNKNOWN lpunk, HRESULT *phr) {
    if (4 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[4]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance5(LPUNKNOWN lpunk, HRESULT *phr) {
    if (5 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[5]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance6(LPUNKNOWN lpunk, HRESULT *phr) {
    if (6 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[6]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreatePropertyPageInstance7(LPUNKNOWN lpunk, HRESULT *phr) {
    if (7 < g_FactoryIndexToCameraName.size()) {
        return CSpoutCamProperties::CreateInstanceForCamera(lpunk, phr, g_FactoryIndexToCameraName[7]);
    }
    return CSpoutCamProperties::CreateInstance(lpunk, phr);
}

// Individual camera factory functions - each creates a specific camera
CUnknown * WINAPI CreateCameraInstance0(LPUNKNOWN lpunk, HRESULT *phr) {
    if (0 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[0];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance1(LPUNKNOWN lpunk, HRESULT *phr) {
    if (1 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[1];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance2(LPUNKNOWN lpunk, HRESULT *phr) {
    if (2 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[2];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance3(LPUNKNOWN lpunk, HRESULT *phr) {
    if (3 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[3];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance4(LPUNKNOWN lpunk, HRESULT *phr) {
    if (4 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[4];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance5(LPUNKNOWN lpunk, HRESULT *phr) {
    if (5 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[5];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance6(LPUNKNOWN lpunk, HRESULT *phr) {
    if (6 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[6];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

CUnknown * WINAPI CreateCameraInstance7(LPUNKNOWN lpunk, HRESULT *phr) {
    if (7 < g_FactoryIndexToCameraName.size()) {
        std::string cameraName = g_FactoryIndexToCameraName[7];
        auto manager = SpoutCam::DynamicCameraManager::GetInstance();
        auto camera = manager->GetCamera(cameraName);
        if (camera) {
            return CVCam::CreateCameraInstance(lpunk, phr, camera->clsid);
        }
    }
    return CVCam::CreateInstance(lpunk, phr);
}

// Dynamic template initialization - creates templates for all active cameras
void InitializeTemplates() {
    g_cTemplates = 0;
    
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    manager->LoadCamerasFromRegistry();
    auto cameras = manager->GetAllCameras();
    
    // Create templates for ALL cameras (not just active ones)
    // This ensures DirectShow can instantiate any registered camera
    int cameraIndex = 0;
    for (auto* camera : cameras) {
        // Store CLSID to camera name mapping
        g_CLSIDToCameraName[camera->clsid] = camera->name;
        
        // Camera filter template with individual factory function
        g_Templates[g_cTemplates].m_Name = new WCHAR[256];
        MultiByteToWideChar(CP_ACP, 0, camera->name.c_str(), -1, 
                           const_cast<WCHAR*>(g_Templates[g_cTemplates].m_Name), 256);
        g_Templates[g_cTemplates].m_ClsID = &camera->clsid;
        
        // Assign individual camera factory function based on camera index
        if (cameraIndex < 8) {
            g_Templates[g_cTemplates].m_lpfnNew = g_CameraFactoryFunctions[cameraIndex];
        } else {
            g_Templates[g_cTemplates].m_lpfnNew = CVCam::CreateInstance;
        }
        
        g_Templates[g_cTemplates].m_lpfnInit = nullptr;
        g_Templates[g_cTemplates].m_pAMovieSetup_Filter = &AMSFilterVCam;
        g_cTemplates++;
        
        // Property page template with camera-specific factory index
        g_Templates[g_cTemplates].m_Name = new WCHAR[256];
        swprintf_s(const_cast<WCHAR*>(g_Templates[g_cTemplates].m_Name), 256, 
                  L"%S Settings", camera->name.c_str());
        g_Templates[g_cTemplates].m_ClsID = &camera->propPageClsid;
        
        // Ensure the camera name is in the factory index array (add once per camera)
        if (cameraIndex >= (int)g_FactoryIndexToCameraName.size()) {
            g_FactoryIndexToCameraName.push_back(camera->name);
        }
        
        // Use the specific factory function for this camera index
        if (cameraIndex < 8) { // We have 8 predefined factory functions
            g_Templates[g_cTemplates].m_lpfnNew = g_PropertyPageFactoryFunctions[cameraIndex];
        } else {
            // Fall back to generic factory for cameras beyond index 7
            g_Templates[g_cTemplates].m_lpfnNew = CreatePropertyPageInstance;
        }
        g_Templates[g_cTemplates].m_lpfnInit = nullptr;
        g_Templates[g_cTemplates].m_pAMovieSetup_Filter = nullptr;
        
        // Store CLSID to camera name mapping for property pages
        g_PropPageCLSIDToCameraName[camera->propPageClsid] = camera->name;
        
        g_cTemplates++;
        
        // Move to next camera
        cameraIndex++;
    }
    
    // If no cameras are active, create a default SpoutCam entry
    if (g_cTemplates == 0) {
        auto defaultCamera = manager->CreateCamera("SpoutCam");
        if (defaultCamera) {
            manager->SetCameraActive("SpoutCam", true);
            
            // Store CLSID to camera name mapping
            g_CLSIDToCameraName[defaultCamera->clsid] = defaultCamera->name;
            
            g_Templates[g_cTemplates].m_Name = L"SpoutCam";
            g_Templates[g_cTemplates].m_ClsID = &defaultCamera->clsid;
            g_Templates[g_cTemplates].m_lpfnNew = CVCam::CreateInstance;
            g_Templates[g_cTemplates].m_lpfnInit = nullptr;
            g_Templates[g_cTemplates].m_pAMovieSetup_Filter = &AMSFilterVCam;
            g_cTemplates++;
            
            g_Templates[g_cTemplates].m_Name = L"SpoutCam Settings";
            g_Templates[g_cTemplates].m_ClsID = &defaultCamera->propPageClsid;
            g_Templates[g_cTemplates].m_lpfnNew = CreatePropertyPageInstance;
            
            // Store mapping for default camera too
            g_PropPageCLSIDToCameraName[defaultCamera->propPageClsid] = defaultCamera->name;
            g_Templates[g_cTemplates].m_lpfnInit = nullptr;
            g_Templates[g_cTemplates].m_pAMovieSetup_Filter = nullptr;
            g_cTemplates++;
        }
    }
}

/*
HRESULT RegisterFilter(
  [in]       REFCLSID clsidFilter,
  [in]       LPCWSTR Name,
  [in, out]  IMoniker **ppMoniker,
  [in]       const CLSID *pclsidCategory,
  [in]       const OLECHAR *szInstance,
  [in]       const REGFILTER2 *prf2
);
*/

STDAPI RegisterFilters( BOOL bRegister )
{
	ASSERT(g_hInst != 0);

    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
	
	//<==================== VS-START ====================>
	if (0 == GetModuleFileNameW(g_hInst, achFileName, MAX_PATH))
	{
		return AmHresultFromWin32(GetLastError());
	}
	//<==================== VS-END ====================>

    hr = CoInitialize(0);
    if(bRegister)
    {
        // Refresh camera names from registry before registration
        InitializeFilterSetups();
        
        // Register all dynamic cameras and their property pages
        for (size_t i = 0; i < g_DynamicCameraConfigs.size() && SUCCEEDED(hr); i++) {
            hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].clsid, CameraNames[i].c_str(), achFileName, L"Both", L"InprocServer32");
            if (SUCCEEDED(hr)) {
                hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
            }
        }
    }

    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject( CLSID_FilterMapper2, IID_IFilterMapper2, fm );
        if( SUCCEEDED(hr) )
        {
            if(bRegister)
            {
                // Register each camera as a video input device
                for (size_t i = 0; i < g_DynamicCameraConfigs.size() && SUCCEEDED(hr); i++) {
                    IMoniker *pMoniker = 0;
                    REGFILTER2 rf2;
                    rf2.dwVersion = 1;
                    rf2.dwMerit = MERIT_DO_NOT_USE;
                    rf2.cPins = 1;
                    rf2.rgPins = &AMSPinVCam;
                    hr = fm->RegisterFilter(g_DynamicCameraConfigs[i].clsid, CameraNames[i].c_str(), &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                }
            }
            else
            {
                // Unregister each camera
                for (size_t i = 0; i < g_DynamicCameraConfigs.size(); i++) {
                    fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_DynamicCameraConfigs[i].clsid);
                }
            }
        }

      // release interface
      if(fm)
          fm->Release();
    }

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// New camera name-based registration functions
STDAPI RegisterCameraByName(const char* cameraName)
{
    printf("RegisterCameraByName: Starting registration for camera '%s'\n", cameraName ? cameraName : "NULL");
    
    if (!cameraName) {
        printf("RegisterCameraByName: ERROR - cameraName is NULL\n");
        return E_INVALIDARG;
    }
    
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    if (!manager) {
        printf("RegisterCameraByName: ERROR - Could not get DynamicCameraManager instance\n");
        return E_FAIL;
    }
    
    auto camera = manager->GetCamera(cameraName);
    if (!camera) {
        printf("RegisterCameraByName: ERROR - Camera '%s' not found in manager\n", cameraName);
        printf("RegisterCameraByName: Available cameras:\n");
        auto allCameras = manager->GetAllCameras();
        for (size_t j = 0; j < allCameras.size(); j++) {
            printf("  [%d] %s\n", (int)j, allCameras[j]->name.c_str());
        }
        return E_FAIL;
    }
    
    printf("RegisterCameraByName: Found camera '%s' in manager\n", cameraName);
    printf("RegisterCameraByName: Camera CLSID: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
        camera->clsid.Data1, camera->clsid.Data2, camera->clsid.Data3,
        camera->clsid.Data4[0], camera->clsid.Data4[1], camera->clsid.Data4[2], camera->clsid.Data4[3],
        camera->clsid.Data4[4], camera->clsid.Data4[5], camera->clsid.Data4[6], camera->clsid.Data4[7]);
    
    // Register camera and property page CLSIDs directly (no index needed)
    WCHAR achFileName[MAX_PATH];
    if (0 == GetModuleFileNameW(g_hInst, achFileName, MAX_PATH)) {
        return AmHresultFromWin32(GetLastError());
    }
    
    HRESULT hr = CoInitialize(0);
    printf("RegisterCameraByName: CoInitialize returned 0x%08X\n", hr);
    
    // Convert camera name to wide string for registration
    wchar_t wideCameraName[256];
    MultiByteToWideChar(CP_ACP, 0, cameraName, -1, wideCameraName, 256);
    
    // Register camera CLSID directly 
    printf("RegisterCameraByName: Registering camera CLSID for '%s'\n", cameraName);
    hr = AMovieSetupRegisterServer(camera->clsid, wideCameraName, achFileName, L"Both", L"InprocServer32");
    printf("RegisterCameraByName: Camera CLSID registration returned 0x%08X (%s)\n", 
        hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
        
    if (SUCCEEDED(hr)) {
        // Register property page CLSID
        printf("RegisterCameraByName: Registering property page CLSID for '%s'\n", cameraName);
        hr = AMovieSetupRegisterServer(camera->propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
        printf("RegisterCameraByName: Property page CLSID registration returned 0x%08X (%s)\n", 
            hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
    }
    
    // CRITICAL: Register the filter in DirectShow VideoInputDeviceCategory
    // This is what makes the camera visible to video applications
    if (SUCCEEDED(hr)) {
        printf("RegisterCameraByName: Registering filter in VideoInputDeviceCategory for '%s'\n", cameraName);
        
        IFilterMapper2 *fm = 0;
        hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void **)&fm);
        printf("RegisterCameraByName: CoCreateInstance(FilterMapper2) returned 0x%08X\n", hr);
        
        if (SUCCEEDED(hr)) {
            IMoniker *pMoniker = 0;
            REGFILTER2 rf2;
            rf2.dwVersion = 1;
            rf2.dwMerit = MERIT_DO_NOT_USE;
            rf2.cPins = 1;
            rf2.rgPins = &AMSPinVCam;
            
            // CRITICAL FIX: Register each camera with its unique CLSID in DirectShow category
            // This ensures each camera appears as a separate video device
            hr = fm->RegisterFilter(camera->clsid, wideCameraName, &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
            printf("RegisterCameraByName: DirectShow RegisterFilter returned 0x%08X for camera '%s'\n", hr, cameraName);
            
            // Also do manual instance-only registration for better visibility
            HRESULT hrManual = RegisterCameraInstanceOnly(camera->clsid, wideCameraName);
            printf("RegisterCameraByName: Manual instance registration returned 0x%08X for camera '%s'\n", hrManual, cameraName);
            
            if (SUCCEEDED(hr)) {
                printf("RegisterCameraByName: Camera '%s' successfully registered as separate DirectShow device\n", cameraName);
            } else {
                printf("RegisterCameraByName: Failed to register camera '%s' in DirectShow category\n", cameraName);
            }
            
            fm->Release();
        }
    }
    
    if (SUCCEEDED(hr)) {
        // Mark camera as active in manager
        manager->SetCameraActive(cameraName, true);
    }
    
    return hr;
}

STDAPI UnregisterCameraByName(const char* cameraName)
{
    printf("UnregisterCameraByName: Starting unregistration for camera '%s'\n", cameraName ? cameraName : "NULL");
    
    if (!cameraName) return E_INVALIDARG;
    
    auto manager = SpoutCam::DynamicCameraManager::GetInstance();
    if (!manager) {
        printf("UnregisterCameraByName: ERROR - Could not get DynamicCameraManager instance\n");
        return E_FAIL;
    }
    
    auto camera = manager->GetCamera(cameraName);
    if (!camera) {
        printf("UnregisterCameraByName: ERROR - Camera '%s' not found in manager\n", cameraName);
        return E_FAIL;
    }
    
    HRESULT hr = CoInitialize(0);
    printf("UnregisterCameraByName: CoInitialize returned 0x%08X\n", hr);
    
    // Unregister camera CLSID directly (no index needed)
    printf("UnregisterCameraByName: Unregistering camera CLSID for '%s'\n", cameraName);
    hr = AMovieSetupUnregisterServer(camera->clsid);
    printf("UnregisterCameraByName: Camera CLSID unregistration returned 0x%08X (%s)\n", 
        hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
        
    // Unregister property page CLSID
    printf("UnregisterCameraByName: Unregistering property page CLSID for '%s'\n", cameraName);
    HRESULT hrProp = AMovieSetupUnregisterServer(camera->propPageClsid);
    printf("UnregisterCameraByName: Property page CLSID unregistration returned 0x%08X (%s)\n", 
        hrProp, SUCCEEDED(hrProp) ? "SUCCESS" : "FAILED");
    
    // CRITICAL: Unregister the filter from DirectShow VideoInputDeviceCategory
    // This removes the camera from video applications
    printf("UnregisterCameraByName: Unregistering filter from VideoInputDeviceCategory for '%s'\n", cameraName);
    
    IFilterMapper2 *fm = 0;
    HRESULT hrFilter = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void **)&fm);
    printf("UnregisterCameraByName: CoCreateInstance(FilterMapper2) returned 0x%08X\n", hrFilter);
    
    if (SUCCEEDED(hrFilter)) {
        // CRITICAL FIX: Unregister from DirectShow category using the camera's unique CLSID
        hrFilter = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, camera->clsid);
        printf("UnregisterCameraByName: DirectShow UnregisterFilter returned 0x%08X for camera '%s'\n", hrFilter, cameraName);
        
        // Also do manual instance-only unregistration
        HRESULT hrManual = UnregisterCameraInstanceOnly(camera->clsid);
        printf("UnregisterCameraByName: Manual instance unregistration returned 0x%08X for camera '%s'\n", hrManual, cameraName);
        
        if (SUCCEEDED(hrFilter)) {
            printf("UnregisterCameraByName: Camera '%s' successfully unregistered from DirectShow\n", cameraName);
        } else {
            printf("UnregisterCameraByName: Failed to unregister camera '%s' from DirectShow category\n", cameraName);
        }
        
        fm->Release();
    }
    
    // Mark camera as inactive in manager
    manager->SetCameraActive(cameraName, false);
    
    // Return success if either succeeded (property page failure is not critical)
    return SUCCEEDED(hr) ? hr : hrProp;
}

HRESULT RegisterSingleCameraFilter( BOOL bRegister, int cameraIndex )
{
	printf("RegisterSingleCameraFilter: Called with bRegister=%s, cameraIndex=%d\n", 
        bRegister ? "TRUE" : "FALSE", cameraIndex);
    
    ASSERT(g_hInst != 0);

    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
	
	//<==================== VS-START ====================>
	if (0 == GetModuleFileNameW(g_hInst, achFileName, MAX_PATH))
	{
		return AmHresultFromWin32(GetLastError());
	}
	//<==================== VS-END ======================>

    hr = CoInitialize(0);
    printf("RegisterSingleCameraFilter: CoInitialize returned 0x%08X\n", hr);
    
    if(bRegister)
    {
        // Refresh camera names from registry before registration
        printf("RegisterSingleCameraFilter: Calling InitializeFilterSetups()\n");
        InitializeFilterSetups();
        printf("RegisterSingleCameraFilter: After InitializeFilterSetups, g_DynamicCameraConfigs.size() = %d\n", 
            (int)g_DynamicCameraConfigs.size());
        
        if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
            // Register single camera and its property page
            printf("RegisterSingleCameraFilter: Registering single camera at index %d\n", cameraIndex);
            printf("RegisterSingleCameraFilter: Camera name: '%ls'\n", CameraNames[cameraIndex].c_str());
            printf("RegisterSingleCameraFilter: Camera CLSID: {%08X-%04X-%04X-...}\n", 
                g_DynamicCameraConfigs[cameraIndex].clsid.Data1, 
                g_DynamicCameraConfigs[cameraIndex].clsid.Data2, 
                g_DynamicCameraConfigs[cameraIndex].clsid.Data3);
            
            hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[cameraIndex].clsid, CameraNames[cameraIndex].c_str(), achFileName, L"Both", L"InprocServer32");
            printf("RegisterSingleCameraFilter: Camera CLSID registration returned 0x%08X (%s)\n", 
                hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
                
            if (SUCCEEDED(hr)) {
                hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[cameraIndex].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
                printf("RegisterSingleCameraFilter: Property page CLSID registration returned 0x%08X (%s)\n", 
                    hr, SUCCEEDED(hr) ? "SUCCESS" : "FAILED");
            }
        } else {
            printf("RegisterSingleCameraFilter: ERROR - cameraIndex %d is out of range (0-%d)\n", 
                cameraIndex, (int)g_DynamicCameraConfigs.size() - 1);
            // Register all cameras and their property pages
            printf("RegisterSingleCameraFilter: Falling back to registering all cameras\n");
            for (size_t i = 0; i < g_DynamicCameraConfigs.size() && SUCCEEDED(hr); i++) {
                printf("RegisterSingleCameraFilter: Registering camera %d: '%ls'\n", 
                    (int)i, CameraNames[i].c_str());
                hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].clsid, CameraNames[i].c_str(), achFileName, L"Both", L"InprocServer32");
                if (SUCCEEDED(hr)) {
                    hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
                }
                printf("RegisterSingleCameraFilter: Camera %d registration result: 0x%08X\n", (int)i, hr);
            }
        }
    }

    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject( CLSID_FilterMapper2, IID_IFilterMapper2, fm );
        if( SUCCEEDED(hr) )
        {
            if(bRegister)
            {
                // Ensure camera names are up to date for filter registration too
                if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
                    // Register single camera as a video input device
                    IMoniker *pMoniker = 0;
                    REGFILTER2 rf2;
                    rf2.dwVersion = 1;
                    rf2.dwMerit = MERIT_DO_NOT_USE;
                    rf2.cPins = 1;
                    rf2.rgPins = &AMSPinVCam;
                    hr = fm->RegisterFilter(g_DynamicCameraConfigs[cameraIndex].clsid, CameraNames[cameraIndex].c_str(), &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                } else {
                    // Register each camera as a video input device
                    for (size_t i = 0; i < g_DynamicCameraConfigs.size() && SUCCEEDED(hr); i++) {
                        IMoniker *pMoniker = 0;
                        REGFILTER2 rf2;
                        rf2.dwVersion = 1;
                        rf2.dwMerit = MERIT_DO_NOT_USE;
                        rf2.cPins = 1;
                        rf2.rgPins = &AMSPinVCam;
                        hr = fm->RegisterFilter(g_DynamicCameraConfigs[i].clsid, CameraNames[i].c_str(), &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                    }
                }
            }
            else
            {
                if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
                    // Unregister single camera
                    fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_DynamicCameraConfigs[cameraIndex].clsid);
                } else {
                    // Unregister each camera
                    for (size_t i = 0; i < g_DynamicCameraConfigs.size(); i++) {
                        fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_DynamicCameraConfigs[i].clsid);
                    }
                }
            }
        }

      // release interface
      if(fm)
          fm->Release();
    }

	if (SUCCEEDED(hr) && !bRegister)
	{
        if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
            // Unregister single camera and property page
            AMovieSetupUnregisterServer(g_DynamicCameraConfigs[cameraIndex].clsid);
            AMovieSetupUnregisterServer(g_DynamicCameraConfigs[cameraIndex].propPageClsid);
        } else {
            // Unregister all cameras and property pages
            for (size_t i = 0; i < g_DynamicCameraConfigs.size(); i++) {
                AMovieSetupUnregisterServer(g_DynamicCameraConfigs[i].clsid);
                AMovieSetupUnregisterServer(g_DynamicCameraConfigs[i].propPageClsid);
            }
        }
	}

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// Register only the primary SpoutCam filter - virtual cameras handled at runtime
HRESULT RegisterPrimaryCameraFilter( BOOL bRegister )
{
	ASSERT(g_hInst != 0);
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
	
	if (0 == GetModuleFileNameW(g_hInst, achFileName, MAX_PATH))
	{
		return AmHresultFromWin32(GetLastError());
	}

    hr = CoInitialize(0);
    
    if(bRegister)
    {
        // Use base SpoutCam CLSID - only register one entry point
        // Virtual cameras will be handled through runtime configuration
        hr = AMovieSetupRegisterServer(CLSID_SpoutCam_Primary, L"SpoutCam", achFileName, L"Both", L"InprocServer32");
        
        if (SUCCEEDED(hr)) {
            // Register single property page
            hr = AMovieSetupRegisterServer(CLSID_SpoutCam_PropPage_Primary, L"SpoutCam Settings", achFileName, L"Both", L"InprocServer32");
        }
    }
    else
    {
        // Unregister only the primary CLSIDs
        AMovieSetupUnregisterServer(CLSID_SpoutCam_Primary);
        AMovieSetupUnregisterServer(CLSID_SpoutCam_PropPage_Primary);
    }

    // Also need to register/unregister in DirectShow VideoInputDeviceCategory
    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void **)&fm);
        
        if (SUCCEEDED(hr)) {
            if (bRegister) {
                IMoniker *pMoniker = 0;
                REGFILTER2 rf2;
                rf2.dwVersion = 1;
                rf2.dwMerit = MERIT_DO_NOT_USE;
                rf2.cPins = 1;
                rf2.rgPins = &AMSPinVCam;
                
                hr = fm->RegisterFilter(CLSID_SpoutCam_Primary, L"SpoutCam", &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
            } else {
                hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_SpoutCam_Primary);
            }
            fm->Release();
        }
    }

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// This function will be called by a set-up application, or when the user runs the Regsvr32.exe tool.
// see http://msdn.microsoft.com/en-us/library/windows/desktop/dd376682%28v=vs.85%29.aspx
STDAPI DllRegisterServer()
{
	// Register only the COM server - individual cameras handled at runtime via SpoutCamSettings
	// This avoids duplicate registrations in VideoInputDeviceCategory
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
	
	if (0 == GetModuleFileNameW(g_hInst, achFileName, MAX_PATH))
	{
		return AmHresultFromWin32(GetLastError());
	}

    hr = CoInitialize(0);
    
    if(SUCCEEDED(hr))
    {
        // Register only the COM server components - no DirectShow filters
        hr = AMovieSetupRegisterServer(CLSID_SpoutCam_Primary, L"SpoutCam", achFileName, L"Both", L"InprocServer32");
        
        if (SUCCEEDED(hr)) {
            // Register single property page
            hr = AMovieSetupRegisterServer(CLSID_SpoutCam_PropPage_Primary, L"SpoutCam Settings", achFileName, L"Both", L"InprocServer32");
        }
    }
    
    CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer()
{
	// Unregister only the COM server components - individual cameras handled by SpoutCamSettings
    HRESULT hr = CoInitialize(0);
    
    if(SUCCEEDED(hr))
    {
        // Unregister only the COM server components
        AMovieSetupUnregisterServer(CLSID_SpoutCam_Primary);
        AMovieSetupUnregisterServer(CLSID_SpoutCam_PropPage_Primary);
    }
    
    CoUninitialize();
    return hr;
}

// External functions for single camera registration
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE RegisterSingleSpoutCamera(int cameraIndex)
{
    return RegisterSingleCameraFilter(TRUE, cameraIndex);
}

extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE UnregisterSingleSpoutCamera(int cameraIndex)
{
    return RegisterSingleCameraFilter(FALSE, cameraIndex);
}

// Manual Instance-only registration functions
HRESULT RegisterCameraInstanceOnly(const GUID& instanceGuid, const WCHAR* friendlyName)
{
    HKEY hDeviceKey = NULL;
    HKEY hInstanceKey = NULL;
    HRESULT hr = E_FAIL;
    
    // Open Video Input Device Category key
    WCHAR deviceCategoryPath[] = L"SOFTWARE\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance";
    
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, deviceCategoryPath, 0, KEY_CREATE_SUB_KEY, &hDeviceKey);
    if (result != ERROR_SUCCESS) {
        printf("RegisterCameraInstanceOnly: Failed to open device category key: %ld\n", result);
        return HRESULT_FROM_WIN32(result);
    }
    
    // Create Instance subkey using camera's unique GUID
    WCHAR instanceGuidStr[64];
    StringFromGUID2(instanceGuid, instanceGuidStr, ARRAYSIZE(instanceGuidStr));
    
    result = RegCreateKeyExW(hDeviceKey, instanceGuidStr, 0, NULL, REG_OPTION_NON_VOLATILE, 
                            KEY_SET_VALUE, NULL, &hInstanceKey, NULL);
    if (result == ERROR_SUCCESS) {
        // Set FriendlyName
        result = RegSetValueExW(hInstanceKey, L"FriendlyName", 0, REG_SZ, 
                               (BYTE*)friendlyName, (DWORD)((wcslen(friendlyName) + 1) * sizeof(WCHAR)));
        
        if (result == ERROR_SUCCESS) {
            // Set CLSID to the specific instance's unique CLSID (NOT shared)
            WCHAR instanceClsidStr[64];
            StringFromGUID2(instanceGuid, instanceClsidStr, ARRAYSIZE(instanceClsidStr));
            result = RegSetValueExW(hInstanceKey, L"CLSID", 0, REG_SZ,
                                   (BYTE*)instanceClsidStr, (DWORD)((wcslen(instanceClsidStr) + 1) * sizeof(WCHAR)));
            
            if (result == ERROR_SUCCESS) {
                hr = S_OK;
                printf("RegisterCameraInstanceOnly: Successfully registered '%ls' as instance %ls\n", 
                       friendlyName, instanceGuidStr);
            } else {
                printf("RegisterCameraInstanceOnly: Failed to set CLSID: %ld\n", result);
            }
        } else {
            printf("RegisterCameraInstanceOnly: Failed to set FriendlyName: %ld\n", result);
        }
        
        RegCloseKey(hInstanceKey);
    } else {
        printf("RegisterCameraInstanceOnly: Failed to create instance key: %ld\n", result);
    }
    
    RegCloseKey(hDeviceKey);
    return hr;
}

HRESULT UnregisterCameraInstanceOnly(const GUID& instanceGuid)
{
    HKEY hDeviceKey = NULL;
    HRESULT hr = E_FAIL;
    
    // Open Video Input Device Category key
    WCHAR deviceCategoryPath[] = L"SOFTWARE\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance";
    
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, deviceCategoryPath, 0, KEY_SET_VALUE, &hDeviceKey);
    if (result != ERROR_SUCCESS) {
        printf("UnregisterCameraInstanceOnly: Failed to open device category key: %ld\n", result);
        return HRESULT_FROM_WIN32(result);
    }
    
    // Delete Instance subkey
    WCHAR instanceGuidStr[64];
    StringFromGUID2(instanceGuid, instanceGuidStr, ARRAYSIZE(instanceGuidStr));
    
    result = RegDeleteKeyW(hDeviceKey, instanceGuidStr);
    if (result == ERROR_SUCCESS) {
        hr = S_OK;
        printf("UnregisterCameraInstanceOnly: Successfully unregistered instance %ls\n", instanceGuidStr);
    } else {
        printf("UnregisterCameraInstanceOnly: Failed to delete instance key %ls: %ld\n", instanceGuidStr, result);
    }
    
    RegCloseKey(hDeviceKey);
    return hr;
}

// External function to get the name that will be used for a camera index
extern "C" __declspec(dllexport) BOOL STDAPICALLTYPE GetSpoutCameraName(int cameraIndex, char* nameBuffer, int bufferSize)
{
    if (cameraIndex < 0 || cameraIndex >= (int)g_DynamicCameraConfigs.size() || !nameBuffer || bufferSize <= 0) {
        return FALSE;
    }
    
    const char* nameToUse;
    
    // Use dynamic camera name
    nameToUse = g_DynamicCameraConfigs[cameraIndex].name;
    
    strncpy_s(nameBuffer, bufferSize, nameToUse, _TRUNCATE);
    return TRUE;
}

// Configure a specific camera instance 
extern "C" __declspec(dllexport) void STDAPICALLTYPE ConfigureSpoutCamera(HWND hwndStub, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	HRESULT hr;
	IBaseFilter *pFilter;
	CUnknown *pInstance;
	int cameraIndex = 0;

	// Allocate console for debugging
	AllocConsole();
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	SetConsoleTitle(L"SpoutCam Configuration Debug");
	
	printf("=== ConfigureSpoutCamera Debug Session ===\n");
	printf("Raw command line: '%s'\n", lpszCmdLine ? lpszCmdLine : "(null)");
	
	// Parse camera index from command line
	if (lpszCmdLine && *lpszCmdLine) {
		cameraIndex = atoi(lpszCmdLine);
		printf("Parsed camera index: %d\n", cameraIndex);
	} else {
		printf("No command line parameter, using default camera 0\n");
		cameraIndex = 0;
	}
	
	// Validate camera index
	if (cameraIndex < 0 || cameraIndex >= (int)g_DynamicCameraConfigs.size()) {
		printf("ERROR: Invalid camera index %d (valid range: 0-%d)\n", cameraIndex, (int)g_DynamicCameraConfigs.size()-1);
		printf("Using camera 0 instead\n");
		cameraIndex = 0;
	}
	
	printf("ConfigureSpoutCamera: Starting configuration for camera %d\n", cameraIndex);

	hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		printf("ConfigureSpoutCamera: CoInitialize failed with HRESULT 0x%08X\n", hr);
		printf("Press any key to exit...\n");
		_getch();
		return;
	}
	printf("ConfigureSpoutCamera: CoInitialize succeeded\n");

	// Create the specific camera instance
	printf("ConfigureSpoutCamera: Creating camera instance %d...\n", cameraIndex);
	if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
		pInstance = CVCam::CreateCameraInstance(nullptr, &hr, g_DynamicCameraConfigs[cameraIndex].clsid);
	} else {
		printf("ConfigureSpoutCamera: Invalid camera index\n");
		hr = E_FAIL;
		pInstance = nullptr;
	}
	if (SUCCEEDED(hr))
	{
		printf("ConfigureSpoutCamera: Camera instance created successfully\n");
		hr = pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
		if (SUCCEEDED(hr))
		{
			printf("ConfigureSpoutCamera: IBaseFilter interface obtained\n");
			// Open the settings dialog for the specific camera
			printf("ConfigureSpoutCamera: Opening property page...\n");
			hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());
			if (SUCCEEDED(hr)) {
				printf("ConfigureSpoutCamera: Property page opened successfully\n");
			} else {
				printf("ConfigureSpoutCamera: ShowFilterPropertyPage failed with HRESULT 0x%08X\n", hr);
				
				// Print common error meanings
				switch (hr) {
					case 0x80040154: printf("  -> Class not registered\n"); break;
					case 0x80004002: printf("  -> No such interface supported\n"); break;
					case 0x80070005: printf("  -> Access denied\n"); break;
					case 0x8007000E: printf("  -> Not enough memory\n"); break;
					case 0x80004005: printf("  -> Unspecified error\n"); break;
					default: printf("  -> Unknown error\n"); break;
				}
			}
			pFilter->Release();
		}
		else {
			printf("ConfigureSpoutCamera: Failed to get IBaseFilter interface, HRESULT 0x%08X\n", hr);
		}
		delete pInstance;
	}
	else {
		printf("ConfigureSpoutCamera: Failed to create camera instance, HRESULT 0x%08X\n", hr);
	}

	printf("ConfigureSpoutCamera: Configuration completed\n");
	printf("\nPress any key to close this debug window...\n");
	_getch(); // Wait for user input before closing console
	
	CoUninitialize();
	FreeConsole();
}

// Configure a specific camera instance by reading from file
extern "C" __declspec(dllexport) void STDAPICALLTYPE ConfigureSpoutCameraFromFile(HWND hwndStub, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	HRESULT hr;
	IBaseFilter *pFilter;
	CUnknown *pInstance;
	std::string cameraName;
	SpoutCam::DynamicCameraConfig* cameraConfig = nullptr;

	// Allocate console for debugging
	AllocConsole();
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	SetConsoleTitle(L"SpoutCam Configuration Debug");
	
	printf("=== ConfigureSpoutCameraFromFile Debug Session ===\n");
	
	// Read camera name from registry (preferred method)
	char selectedCameraName[256] = {0};
	if (ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "SelectedCameraName", selectedCameraName)) {
		cameraName = selectedCameraName;
		printf("Read camera name from registry: '%s'\n", cameraName.c_str());
	} else {
		// Fallback: get first available camera or use command line parameter
		if (lpszCmdLine && strlen(lpszCmdLine) > 0) {
			cameraName = lpszCmdLine;
			printf("Using camera name from command line: '%s'\n", cameraName.c_str());
		} else {
			// Get first available camera
			auto manager = SpoutCam::DynamicCameraManager::GetInstance();
			auto cameras = manager->GetAllCameras();
			if (!cameras.empty()) {
				cameraName = cameras[0]->name;
				printf("No camera specified, using first available camera: '%s'\n", cameraName.c_str());
			} else {
				printf("ERROR: No cameras available in system\n");
				printf("Press any key to exit...\n");
				_getch();
				CoUninitialize();
				return;
			}
		}
	}
	
	// Get camera configuration from DynamicCameraManager
	auto manager = SpoutCam::DynamicCameraManager::GetInstance();
	cameraConfig = manager->GetCamera(cameraName);
	if (!cameraConfig) {
		printf("ERROR: Camera '%s' not found in system\n", cameraName.c_str());
		printf("Available cameras:\n");
		auto cameras = manager->GetAllCameras();
		for (auto camera : cameras) {
			printf("  - %s\n", camera->name.c_str());
		}
		printf("Press any key to exit...\n");
		_getch();
		CoUninitialize();
		return;
	}
	
	printf("ConfigureSpoutCameraFromFile: Starting configuration for camera '%s'\n", cameraName.c_str());

	hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		printf("ConfigureSpoutCameraFromFile: CoInitialize failed with HRESULT 0x%08X\n", hr);
		printf("Press any key to exit...\n");
		_getch();
		return;
	}
	printf("ConfigureSpoutCameraFromFile: CoInitialize succeeded\n");

	// Create the specific camera instance
	printf("ConfigureSpoutCameraFromFile: Creating camera instance for '%s'...\n", cameraName.c_str());
	pInstance = CVCam::CreateCameraInstance(nullptr, &hr, cameraConfig->clsid);
	
	if (SUCCEEDED(hr))
	{
		printf("ConfigureSpoutCameraFromFile: Camera instance created successfully\n");
		hr = pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
		if (SUCCEEDED(hr))
		{
			printf("ConfigureSpoutCameraFromFile: IBaseFilter interface obtained\n");
			
			// Check if the filter has a property page
			ISpecifyPropertyPages *pProp = NULL;
			hr = pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pProp);
			if (SUCCEEDED(hr)) {
				printf("ConfigureSpoutCameraFromFile: Filter supports property pages\n");
				pProp->Release();
				
				// Try to open property page using standard DirectShow method
				printf("ConfigureSpoutCameraFromFile: Opening property page...\n");
				hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());
				if (SUCCEEDED(hr)) {
					printf("ConfigureSpoutCameraFromFile: Property page opened successfully\n");
				} else {
					printf("ConfigureSpoutCameraFromFile: ShowFilterPropertyPage failed with HRESULT 0x%08X\n", hr);
					printf("ConfigureSpoutCameraFromFile: Trying alternative approach with specific camera CLSID...\n");
					
					// Try using the specific camera's property page CLSID
					auto manager = SpoutCam::DynamicCameraManager::GetInstance();
					auto cameraConfig = manager->GetCamera(cameraName);
					
					if (cameraConfig) {
						printf("ConfigureSpoutCameraFromFile: Using property page CLSID for camera '%s'\n", cameraName.c_str());
						hr = OleCreatePropertyFrame(
							GetDesktopWindow(),						// Parent window
							0, 0,									// Reserved
							L"SpoutCam Properties",					// Caption
							1,										// Number of objects
							(IUnknown**)&pFilter,					// Array of objects
							1,										// Number of pages
							&cameraConfig->propPageClsid,			// Array of page CLSIDs
							0,										// Locale ID
							0,										// Reserved
							NULL									// Reserved
						);
						if (SUCCEEDED(hr)) {
							printf("ConfigureSpoutCameraFromFile: OleCreatePropertyFrame succeeded\n");
						} else {
							printf("ConfigureSpoutCameraFromFile: OleCreatePropertyFrame also failed with HRESULT 0x%08X\n", hr);
						}
					} else {
						printf("ConfigureSpoutCameraFromFile: Could not find camera configuration for '%s'\n", cameraName.c_str());
					}
				}
					
					// If both methods failed, show the original error message
					if (FAILED(hr)) {
						MessageBoxA(GetDesktopWindow(), 
							"Cannot open camera configuration.\n\n"
							"This happens when property page CLSIDs are not properly registered.\n\n"
							"Solution:\n"
							"1. Try re-registering the camera using SpoutCamSettings.exe\n"
							"2. If problem persists, restart the application\n\n"
							"The camera filter is registered but property pages may need re-registration.",
							"SpoutCam Configuration Error", 
							MB_OK | MB_ICONINFORMATION);
				}
			} else {
				printf("ConfigureSpoutCameraFromFile: Filter does not support property pages, HRESULT 0x%08X\n", hr);
			}
			pFilter->Release();
		}
		else {
			printf("ConfigureSpoutCameraFromFile: Failed to get IBaseFilter interface, HRESULT 0x%08X\n", hr);
		}
		delete pInstance;
	}
	else {
		printf("ConfigureSpoutCameraFromFile: Failed to create camera instance, HRESULT 0x%08X\n", hr);
	}

	printf("ConfigureSpoutCameraFromFile: Configuration completed\n");
	printf("\nPress any key to close this debug window...\n");
	_getch(); // Wait for user input before closing console
	
	CoUninitialize();
	FreeConsole();
}

//<==================== VS-START ====================>
#ifdef DIALOG_WITHOUT_REGISTRATION

extern "C" {
	HRESULT WINAPI OleCreatePropertyFrameDirect(
		HWND hwndOwner,
		LPCOLESTR lpszCaption,
		LPUNKNOWN* ppUnk,
		IPropertyPage * page);
}

HRESULT ShowFilterPropertyPageDirect(IBaseFilter *pFilter, HWND hwndParent)
{
	TRACE("ShowFilterPropertyPageDirect");
	HRESULT hr;
	if (!pFilter)
		return E_POINTER;
	CSpoutCamProperties * pPage = (CSpoutCamProperties *)CSpoutCamProperties::CreateInstance(pFilter, &hr);
	if (SUCCEEDED(hr))
	{
		hr = OleCreatePropertyFrameDirect(
			hwndParent,             // Parent window
			L"SpoutCam",            // Caption for the dialog box - DirectShow will append "Properties"
			(IUnknown **)&pFilter,  // Pointer to the filter
			pPage
		);
		pPage->Release();
	}
	return hr;
}
#endif

void CALLBACK Configure()
{
	HRESULT hr;
	IBaseFilter *pFilter;
	CUnknown *pInstance;

	// Allocate console for debugging
	AllocConsole();
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	
	printf("Configure: Starting configuration for default camera (camera 0)\n");

	hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		printf("Configure: CoInitialize failed with HRESULT 0x%08X\n", hr);
		return;
	}
	printf("Configure: CoInitialize succeeded\n");

	// Obtain the filter's IBaseFilter interface.
	printf("Configure: Creating default camera instance...\n");
	pInstance = CVCam::CreateInstance(nullptr, &hr);
	if (SUCCEEDED(hr))
	{
		printf("Configure: Default camera instance created successfully\n");
		hr = pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
		if (SUCCEEDED(hr))
		{
			printf("Configure: IBaseFilter interface obtained\n");
			// If the filter is registered, this will open the settings dialog.
			printf("Configure: Opening property page...\n");
			hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());
			if (SUCCEEDED(hr)) {
				printf("Configure: Property page opened successfully\n");
			} else {
				printf("Configure: ShowFilterPropertyPage failed with HRESULT 0x%08X\n", hr);
			}

#ifdef DIALOG_WITHOUT_REGISTRATION
			if (FAILED(hr))
			{
				printf("Configure: Trying ShowFilterPropertyPageDirect as fallback...\n");
				// The filter propably isn't registered in the system;
				// This will open the settings dialog anyway.
				hr = ShowFilterPropertyPageDirect(pFilter, GetDesktopWindow());
				if (SUCCEEDED(hr)) {
					printf("Configure: ShowFilterPropertyPageDirect succeeded\n");
				} else {
					printf("Configure: ShowFilterPropertyPageDirect also failed with HRESULT 0x%08X\n", hr);
				}
			}
#endif
			pFilter->Release();
		}
		else {
			printf("Configure: Failed to get IBaseFilter interface, HRESULT 0x%08X\n", hr);
		}
		delete pInstance;
	}
	else {
		printf("Configure: Failed to create default camera instance, HRESULT 0x%08X\n", hr);
	}

	printf("Configure: Configuration completed\n");
	printf("Press any key to close console...\n");
	_getch(); // Wait for user input before closing console

	CoUninitialize();
}
//<==================== VS-END ======================>

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        // Initialize the templates when DLL is loaded
        InitializeTemplates();
    }
    // No cleanup needed for static array
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
