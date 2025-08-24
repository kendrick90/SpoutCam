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

#include "cam.h"

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
// MULTIPLE CAMERA SUPPORT - AUTOMATED CLSID GENERATION
//
// The system now automatically generates unique CLSIDs for each camera.
// Configure the number of cameras by changing MAX_SPOUT_CAMERAS in cam.h
//
// Camera configurations with unique CLSIDs and names
SpoutCamConfig g_CameraConfigs[MAX_SPOUT_CAMERAS] = {
    // Camera 1 - Original SpoutCam
    {
        "SpoutCam",
        {0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33},
        {0xcd7780b7, 0x40d2, 0x4f33, 0x80, 0xe2, 0xb0, 0x2e, 0x0, 0x9c, 0xe6, 0x3f}
    },
    // Camera 2 - SpoutCam2
    {
        "SpoutCam2", 
        {0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x34},
        {0xcd7780b7, 0x40d2, 0x4f33, 0x80, 0xe2, 0xb0, 0x2e, 0x0, 0x9c, 0xe6, 0x40}
    },
    // Camera 3 - SpoutCam3
    {
        "SpoutCam3",
        {0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x35},
        {0xcd7780b7, 0x40d2, 0x4f33, 0x80, 0xe2, 0xb0, 0x2e, 0x0, 0x9c, 0xe6, 0x41}
    },
    // Camera 4 - SpoutCam4
    {
        "SpoutCam4",
        {0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x36},
        {0xcd7780b7, 0x40d2, 0x4f33, 0x80, 0xe2, 0xb0, 0x2e, 0x0, 0x9c, 0xe6, 0x42}
    }
};

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

// Filter setup arrays for each camera
AMOVIESETUP_FILTER AMSFilters[MAX_SPOUT_CAMERAS];
WCHAR CameraNames[MAX_SPOUT_CAMERAS][64];

// Initialize filter setups
void InitializeFilterSetups() {
    for (int i = 0; i < MAX_SPOUT_CAMERAS; i++) {
        // Convert camera name to wide string
        MultiByteToWideChar(CP_ACP, 0, g_CameraConfigs[i].name, -1, CameraNames[i], 64);
        
        AMSFilters[i].clsID = &g_CameraConfigs[i].clsid;
        AMSFilters[i].strName = CameraNames[i];
        AMSFilters[i].dwMerit = MERIT_DO_NOT_USE;
        AMSFilters[i].nPins = 1;
        AMSFilters[i].lpPin = &AMSPinVCam;
    }
}

// Forward declarations of camera factory functions
extern CUnknown * WINAPI CreateCamera0(LPUNKNOWN lpunk, HRESULT *phr);
extern CUnknown * WINAPI CreateCamera1(LPUNKNOWN lpunk, HRESULT *phr);
extern CUnknown * WINAPI CreateCamera2(LPUNKNOWN lpunk, HRESULT *phr);
extern CUnknown * WINAPI CreateCamera3(LPUNKNOWN lpunk, HRESULT *phr);

// Array of factory functions
static LPFNNewCOMObject CameraFactories[MAX_SPOUT_CAMERAS] = {
    CreateCamera0,
    CreateCamera1,
    CreateCamera2,
    CreateCamera3
};

// Static factory templates array - DirectShow requires this to be statically defined
CFactoryTemplate g_Templates[MAX_SPOUT_CAMERAS * 2]; // Each camera + its property page
int g_cTemplates = MAX_SPOUT_CAMERAS * 2;

// Initialize templates
void InitializeTemplates() {
    InitializeFilterSetups();
    
    // Create templates for each camera and its property page
    for (int i = 0; i < MAX_SPOUT_CAMERAS; i++) {
        // Camera template
        g_Templates[i * 2].m_Name = CameraNames[i];
        g_Templates[i * 2].m_ClsID = &g_CameraConfigs[i].clsid;
        g_Templates[i * 2].m_lpfnNew = CameraFactories[i];
        g_Templates[i * 2].m_lpfnInit = nullptr;
        g_Templates[i * 2].m_pAMovieSetup_Filter = &AMSFilters[i];
        
        // Property page template
        g_Templates[i * 2 + 1].m_Name = L"Settings";
        g_Templates[i * 2 + 1].m_ClsID = &g_CameraConfigs[i].propPageClsid;
        g_Templates[i * 2 + 1].m_lpfnNew = CSpoutCamProperties::CreateInstance;
        g_Templates[i * 2 + 1].m_lpfnInit = nullptr;
        g_Templates[i * 2 + 1].m_pAMovieSetup_Filter = nullptr;
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
        // Register all cameras and their property pages
        for (int i = 0; i < MAX_SPOUT_CAMERAS && SUCCEEDED(hr); i++) {
            hr = AMovieSetupRegisterServer(g_CameraConfigs[i].clsid, CameraNames[i], achFileName, L"Both", L"InprocServer32");
            if (SUCCEEDED(hr)) {
                hr = AMovieSetupRegisterServer(g_CameraConfigs[i].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
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
                for (int i = 0; i < MAX_SPOUT_CAMERAS && SUCCEEDED(hr); i++) {
                    IMoniker *pMoniker = 0;
                    REGFILTER2 rf2;
                    rf2.dwVersion = 1;
                    rf2.dwMerit = MERIT_DO_NOT_USE;
                    rf2.cPins = 1;
                    rf2.rgPins = &AMSPinVCam;
                    hr = fm->RegisterFilter(g_CameraConfigs[i].clsid, CameraNames[i], &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                }
            }
            else
            {
                // Unregister each camera
                for (int i = 0; i < MAX_SPOUT_CAMERAS; i++) {
                    fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_CameraConfigs[i].clsid);
                }
            }
        }

      // release interface
      if(fm)
          fm->Release();
    }

    return RegisterSingleCameraFilter(bRegister, -1); // Register all cameras (legacy behavior)
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
        if (cameraIndex >= 0 && cameraIndex < MAX_SPOUT_CAMERAS) {
            // Register single camera and its property page
            hr = AMovieSetupRegisterServer(g_CameraConfigs[cameraIndex].clsid, CameraNames[cameraIndex], achFileName, L"Both", L"InprocServer32");
            if (SUCCEEDED(hr)) {
                hr = AMovieSetupRegisterServer(g_CameraConfigs[cameraIndex].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
            }
        } else {
            // Register all cameras and their property pages
            for (int i = 0; i < MAX_SPOUT_CAMERAS && SUCCEEDED(hr); i++) {
                hr = AMovieSetupRegisterServer(g_CameraConfigs[i].clsid, CameraNames[i], achFileName, L"Both", L"InprocServer32");
                if (SUCCEEDED(hr)) {
                    hr = AMovieSetupRegisterServer(g_CameraConfigs[i].propPageClsid, L"Settings", achFileName, L"Both", L"InprocServer32");
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
                if (cameraIndex >= 0 && cameraIndex < MAX_SPOUT_CAMERAS) {
                    // Register single camera as a video input device
                    IMoniker *pMoniker = 0;
                    REGFILTER2 rf2;
                    rf2.dwVersion = 1;
                    rf2.dwMerit = MERIT_DO_NOT_USE;
                    rf2.cPins = 1;
                    rf2.rgPins = &AMSPinVCam;
                    hr = fm->RegisterFilter(g_CameraConfigs[cameraIndex].clsid, CameraNames[cameraIndex], &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                } else {
                    // Register each camera as a video input device
                    for (int i = 0; i < MAX_SPOUT_CAMERAS && SUCCEEDED(hr); i++) {
                        IMoniker *pMoniker = 0;
                        REGFILTER2 rf2;
                        rf2.dwVersion = 1;
                        rf2.dwMerit = MERIT_DO_NOT_USE;
                        rf2.cPins = 1;
                        rf2.rgPins = &AMSPinVCam;
                        hr = fm->RegisterFilter(g_CameraConfigs[i].clsid, CameraNames[i], &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                    }
                }
            }
            else
            {
                if (cameraIndex >= 0 && cameraIndex < MAX_SPOUT_CAMERAS) {
                    // Unregister single camera
                    fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_CameraConfigs[cameraIndex].clsid);
                } else {
                    // Unregister each camera
                    for (int i = 0; i < MAX_SPOUT_CAMERAS; i++) {
                        fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, g_CameraConfigs[i].clsid);
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
        if (cameraIndex >= 0 && cameraIndex < MAX_SPOUT_CAMERAS) {
            // Unregister single camera and property page
            AMovieSetupUnregisterServer(g_CameraConfigs[cameraIndex].clsid);
            AMovieSetupUnregisterServer(g_CameraConfigs[cameraIndex].propPageClsid);
        } else {
            // Unregister all cameras and property pages
            for (int i = 0; i < MAX_SPOUT_CAMERAS; i++) {
                AMovieSetupUnregisterServer(g_CameraConfigs[i].clsid);
                AMovieSetupUnregisterServer(g_CameraConfigs[i].propPageClsid);
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

	CoInitialize(nullptr);

	// Obtain the filter's IBaseFilter interface.
	pInstance = CVCam::CreateInstance(nullptr, &hr);
	if (SUCCEEDED(hr))
	{
		hr = pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
		if (SUCCEEDED(hr))
		{
			// If the filter is registered, this will open the settings dialog.
			hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());

#ifdef DIALOG_WITHOUT_REGISTRATION
			if (FAILED(hr))
			{
				// The filter propably isn't registered in the system;
				// This will open the settings dialog anyway.
				hr = ShowFilterPropertyPageDirect(pFilter, GetDesktopWindow());
			}
#endif
			pFilter->Release();
		}
		delete pInstance;
	}

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
