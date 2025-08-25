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

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer( CLSID clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );
HRESULT RegisterSingleCameraFilter( BOOL bRegister, int cameraIndex );

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

// Factory function wrapper that finds camera by CLSID
CUnknown * WINAPI CreateCameraByIndex(int index, LPUNKNOWN lpunk, HRESULT *phr) {
    if (index >= 0 && index < (int)g_DynamicCameraConfigs.size()) {
        return CVCam::CreateCameraInstance(lpunk, phr, g_DynamicCameraConfigs[index].clsid);
    }
    *phr = E_FAIL;
    return nullptr;
}

// Individual factory functions for each potential camera (DirectShow needs static functions)
#define MAKE_CAMERA_FACTORY(n) \
CUnknown * WINAPI CreateCamera##n(LPUNKNOWN lpunk, HRESULT *phr) { \
    return CreateCameraByIndex(n, lpunk, phr); \
}

// Generate factory functions for up to 128 cameras
MAKE_CAMERA_FACTORY(0)  MAKE_CAMERA_FACTORY(1)  MAKE_CAMERA_FACTORY(2)  MAKE_CAMERA_FACTORY(3)
MAKE_CAMERA_FACTORY(4)  MAKE_CAMERA_FACTORY(5)  MAKE_CAMERA_FACTORY(6)  MAKE_CAMERA_FACTORY(7)
MAKE_CAMERA_FACTORY(8)  MAKE_CAMERA_FACTORY(9)  MAKE_CAMERA_FACTORY(10) MAKE_CAMERA_FACTORY(11)
MAKE_CAMERA_FACTORY(12) MAKE_CAMERA_FACTORY(13) MAKE_CAMERA_FACTORY(14) MAKE_CAMERA_FACTORY(15)

// Array of factory function pointers
static LPFNNewCOMObject g_CameraFactories[] = {
    CreateCamera0,  CreateCamera1,  CreateCamera2,  CreateCamera3,
    CreateCamera4,  CreateCamera5,  CreateCamera6,  CreateCamera7,
    CreateCamera8,  CreateCamera9,  CreateCamera10, CreateCamera11,
    CreateCamera12, CreateCamera13, CreateCamera14, CreateCamera15
};

// Initialize templates dynamically
void InitializeTemplates() {
    InitializeFilterSetups();
    
    g_cTemplates = 0;
    
    // Create templates for each camera and its property page
    for (size_t i = 0; i < g_DynamicCameraConfigs.size() && i < 16; i++) {
        // Camera template
        g_Templates[g_cTemplates].m_Name = CameraNames[i].c_str();
        g_Templates[g_cTemplates].m_ClsID = &g_DynamicCameraConfigs[i].clsid;
        g_Templates[g_cTemplates].m_lpfnNew = g_CameraFactories[i];
        g_Templates[g_cTemplates].m_lpfnInit = nullptr;
        g_Templates[g_cTemplates].m_pAMovieSetup_Filter = &AMSFilters[i];
        g_cTemplates++;
        
        // Property page template
        g_Templates[g_cTemplates].m_Name = L"Settings";
        g_Templates[g_cTemplates].m_ClsID = &g_DynamicCameraConfigs[i].propPageClsid;
        g_Templates[g_cTemplates].m_lpfnNew = CSpoutCamProperties::CreateInstance;
        g_Templates[g_cTemplates].m_lpfnInit = nullptr;
        g_Templates[g_cTemplates].m_pAMovieSetup_Filter = nullptr;
        g_cTemplates++;
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

HRESULT RegisterSingleCameraFilter( BOOL bRegister, int cameraIndex )
{
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
    if(bRegister)
    {
        // Refresh camera names from registry before registration
        InitializeFilterSetups();
        
        if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
            // Register single camera and its property page
            hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[cameraIndex].clsid, CameraNames[cameraIndex].c_str(), achFileName, L"Both", L"InprocServer32");
            if (SUCCEEDED(hr)) {
                hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[cameraIndex].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
            }
        } else {
            // Register all cameras and their property pages
            for (size_t i = 0; i < g_DynamicCameraConfigs.size() && SUCCEEDED(hr); i++) {
                hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].clsid, CameraNames[i].c_str(), achFileName, L"Both", L"InprocServer32");
                if (SUCCEEDED(hr)) {
                    hr = AMovieSetupRegisterServer(g_DynamicCameraConfigs[i].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
                }
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

// This function will be called by a set-up application, or when the user runs the Regsvr32.exe tool.
// see http://msdn.microsoft.com/en-us/library/windows/desktop/dd376682%28v=vs.85%29.aspx
STDAPI DllRegisterServer()
{
	// Register only the first camera by default for backward compatibility
	// Additional cameras can be registered individually through the properties dialog
	return RegisterSingleCameraFilter(TRUE, 0);
}

STDAPI DllUnregisterServer()
{
	return RegisterFilters(FALSE);
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

// External function to get the name that will be used for a camera index
extern "C" __declspec(dllexport) BOOL STDAPICALLTYPE GetSpoutCameraName(int cameraIndex, char* nameBuffer, int bufferSize)
{
    if (cameraIndex < 0 || cameraIndex >= (int)g_DynamicCameraConfigs.size() || !nameBuffer || bufferSize <= 0) {
        return FALSE;
    }
    
    char customName[256];
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
	int cameraIndex = 0;

	// Allocate console for debugging
	AllocConsole();
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	SetConsoleTitle(L"SpoutCam Configuration Debug");
	
	printf("=== ConfigureSpoutCameraFromFile Debug Session ===\n");
	
	// Read camera index from registry
	DWORD dwCameraIndex = 0;
	if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "SelectedCameraIndex", &dwCameraIndex)) {
		cameraIndex = (int)dwCameraIndex;
		printf("Read camera index from registry: %d\n", cameraIndex);
	} else {
		printf("Could not read camera index from registry, using default camera 0\n");
		cameraIndex = 0;
	}
	
	// Validate camera index
	if (cameraIndex < 0 || cameraIndex >= (int)g_DynamicCameraConfigs.size()) {
		printf("ERROR: Invalid camera index %d (valid range: 0-%d)\n", cameraIndex, (int)g_DynamicCameraConfigs.size()-1);
		printf("Using camera 0 instead\n");
		cameraIndex = 0;
	}
	
	printf("ConfigureSpoutCameraFromFile: Starting configuration for camera %d\n", cameraIndex);

	hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		printf("ConfigureSpoutCameraFromFile: CoInitialize failed with HRESULT 0x%08X\n", hr);
		printf("Press any key to exit...\n");
		_getch();
		return;
	}
	printf("ConfigureSpoutCameraFromFile: CoInitialize succeeded\n");

	// Create the specific camera instance
	printf("ConfigureSpoutCameraFromFile: Creating camera instance %d...\n", cameraIndex);
	if (cameraIndex >= 0 && cameraIndex < (int)g_DynamicCameraConfigs.size()) {
		pInstance = CVCam::CreateCameraInstance(nullptr, &hr, g_DynamicCameraConfigs[cameraIndex].clsid);
	} else {
		printf("ConfigureSpoutCameraFromFile: Invalid camera index\n");
		hr = E_FAIL;
		pInstance = nullptr;
	}
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
				
				// Try to show property page using DirectShow method
				printf("ConfigureSpoutCameraFromFile: Opening property page...\n");
				hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());
				if (SUCCEEDED(hr)) {
					printf("ConfigureSpoutCameraFromFile: Property page opened successfully\n");
				} else {
					printf("ConfigureSpoutCameraFromFile: ShowFilterPropertyPage failed with HRESULT 0x%08X\n", hr);
					printf("ConfigureSpoutCameraFromFile: This usually means no SpoutCam cameras are registered in the system\n");
					printf("ConfigureSpoutCameraFromFile: Solution: Register at least one camera first, then configuration will work\n");
					printf("ConfigureSpoutCameraFromFile: Use SpoutCamSettings.exe to register cameras\n");
					
					// Show user-friendly error message
					MessageBoxA(GetDesktopWindow(), 
						"Cannot open camera configuration.\n\n"
						"This happens when no SpoutCam cameras are registered in the system.\n\n"
						"Solution:\n"
						"1. Open SpoutCamSettings.exe\n"
						"2. Click 'Register' for at least one camera\n"
						"3. Then 'Configure' will work for all cameras\n\n"
						"Once any camera is registered, you can configure any camera (even unregistered ones).",
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
