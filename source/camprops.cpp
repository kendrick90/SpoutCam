#include <stdio.h>
#include <conio.h>
#include <olectl.h>
#include <dshow.h>
#include <commctrl.h>

#include "cam.h"

#include "camprops.h"
#include "resource.h"
#include "version.h"


// For dialog drawing
static HBRUSH g_hbrBkgnd = NULL;

// Legacy tab control variables removed - tabs are no longer used

// Initialize properties dialog with current camera settings
void CSpoutCamProperties::InitializeProps()
{
	// Load and display the current camera name in the edit field
	HWND hwndCameraName = GetDlgItem(this->m_Dlg, IDC_CAMERA_NAME_EDIT);
	if (hwndCameraName && !m_cameraName.empty()) {
		// Convert camera name to wide string and display it
		wchar_t wideCameraName[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wideCameraName, 256);
		Edit_SetText(hwndCameraName, wideCameraName);
	}
	
	// Initialize other camera-specific settings
	UpdateCameraDisplay();
}

// CreateInstance
// Used by the DirectShow base classes to create instances
CUnknown *CSpoutCamProperties::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
	ASSERT(phr);
	CUnknown *punk = new CSpoutCamProperties(lpunk, phr);
	if (punk == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
	}
	return punk;
} // CreateInstance

// Camera-specific factory function that knows which camera it represents
CUnknown *CSpoutCamProperties::CreateInstanceForCamera(LPUNKNOWN lpunk, HRESULT *phr, const std::string& cameraName)
{
	ASSERT(phr);
	CSpoutCamProperties* props = new CSpoutCamProperties(lpunk, phr);
	if (props == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
		return nullptr;
	}
	
	// Set the camera name so the property page knows which camera it represents
	props->m_cameraName = cameraName;
	
	return props;
}

// Constructor
CSpoutCamProperties::CSpoutCamProperties(LPUNKNOWN pUnk, HRESULT *phr) :
	CBasePropertyPage("SpoutCam Configuration", pUnk, IDD_SPOUTCAMBOX, IDS_TITLE),
	m_pCamSettings(NULL),
	m_bIsInitialized(FALSE),
	m_cameraIndex(0), // Default to camera 0, will be updated in OnConnect
	m_cameraName("") // Will be set by camera-specific factory or determined in OnConnect
{
	ASSERT(phr);
	if (phr)
		*phr = S_OK;
	
	OutputDebugStringA("CSpoutCamProperties: Constructor called");
}

//OnReceiveMessage is called when the dialog receives a window message.
INT_PTR CSpoutCamProperties::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HCURSOR cursorHand = NULL;
	LPDRAWITEMSTRUCT lpdis;
	std::wstring wstr;

	switch (uMsg)
	{
		// Owner draw button
	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			
			// Make disable warnings light grey
			if (GetDlgItem(hwnd, IDC_SILENT) == (HWND)lParam) {
				SetTextColor(hdcStatic, RGB(128, 128, 128));
				SetBkColor(hdcStatic, RGB(240, 240, 240));
				if (g_hbrBkgnd == NULL)
					g_hbrBkgnd = CreateSolidBrush(RGB(240, 240, 240));
			}

			// Make version light grey
			if (GetDlgItem(hwnd, IDC_VERS) == (HWND)lParam) {
				SetTextColor(hdcStatic, RGB(128, 128, 128));
				SetBkColor(hdcStatic, RGB(240, 240, 240));
				if (g_hbrBkgnd == NULL)
					g_hbrBkgnd = CreateSolidBrush(RGB(240, 240, 240));
			}

			return (INT_PTR)g_hbrBkgnd;
		}
		break;

	case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lParam;
			if (lpdis->itemID == -1) break;
			switch (lpdis->CtlID) {
				// The blue hyperlink
				case IDC_SPOUT_URL:
					SetTextColor(lpdis->hDC, RGB(6, 69, 173));
					DrawText(lpdis->hDC, L"https://spout.zeal.co", -1, &lpdis->rcItem, DT_LEFT);
					break;

				default:
					break;
			}
			break;

		case WM_INITDIALOG:
			// Set custom window title
			SetWindowTextW(hwnd, L"SpoutCam Settings");
			
			// Center the window on screen using work area
			{
				RECT windowRect;
				GetWindowRect(hwnd, &windowRect);
				
				// Get the work area (screen minus taskbar)
				MONITORINFO mi = { sizeof(mi) };
				GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
				
				int windowWidth = windowRect.right - windowRect.left;
				int windowHeight = windowRect.bottom - windowRect.top;
				int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - windowWidth) / 2;
				int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - windowHeight) / 2;
				
				SetWindowPos(hwnd, HWND_TOP, centerX, centerY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			}
			
			// Hyperlink hand cursor
			cursorHand = LoadCursor(NULL, IDC_HAND);
			SetClassLongPtr(GetDlgItem(hwnd, IDC_SPOUT_URL), GCLP_HCURSOR, (LONG_PTR)cursorHand);
			break;

		case WM_SHOWWINDOW:
			if (wParam) // If showing the window
			{
				// Center the window when it's about to be shown
				RECT windowRect;
				GetWindowRect(hwnd, &windowRect);
				
				MONITORINFO mi = { sizeof(mi) };
				GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
				
				int windowWidth = windowRect.right - windowRect.left;
				int windowHeight = windowRect.bottom - windowRect.top;
				int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - windowWidth) / 2;
				int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - windowHeight) / 2;
				
				SetWindowPos(hwnd, HWND_TOP, centerX, centerY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			break;

		case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {

				case IDC_SPOUT_URL :
					ShellExecute(hwnd, L"open", L"http://spout.zeal.co", NULL, NULL, SW_SHOWNORMAL);
					break;
				
				case IDC_SILENT:
					// Toggle warning silent mode
					if (IsDlgButtonChecked(hwnd, IDC_SILENT) == BST_CHECKED)
						m_bSilent = TRUE;
					else
						m_bSilent = FALSE;
					break;

				case IDC_MIRROR:
					// Apply mirror setting immediately
					ApplyRealTimeSettings();
					break;

				case IDC_FLIP:
					// Apply flip setting immediately  
					ApplyRealTimeSettings();
					break;

				case IDC_SWAP:
					// Apply swap setting immediately
					ApplyRealTimeSettings();
					break;

				case IDC_REFRESH:
					// Refresh the available senders list
					RefreshSenderList();
					break;

				case IDC_REGISTER:
					// Register this specific camera
					{
						int cameraIndex = 0;
						GetCameraIndex(&cameraIndex);
						RegisterSingleCamera(cameraIndex);
					}
					break;

				case IDC_UNREGISTER:
					// Unregister this specific camera
					{
						int cameraIndex = 0;
						GetCameraIndex(&cameraIndex);
						UnregisterSingleCamera(cameraIndex);
					}
					break;

				case IDC_REREGISTER:
					// Reregister this specific camera (unregister then register)
					{
						int cameraIndex = 0;
						GetCameraIndex(&cameraIndex);
						
						// First unregister
						UnregisterSingleCamera(cameraIndex);
						
						// Brief pause to ensure unregistration is complete
						Sleep(500);
						
						// Then register again
						RegisterSingleCamera(cameraIndex);
					}
					break;



				case IDC_SENDER_LIST:
					// Handle sender selection change
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						HWND hwndSenderList = GetDlgItem(hwnd, IDC_SENDER_LIST);
						int selectedIndex = ComboBox_GetCurSel(hwndSenderList);
						
						if (selectedIndex > 0) { // Not "Auto"
							// Get the selected sender name and put it in the custom name field
							WCHAR senderName[256];
							ComboBox_GetLBText(hwndSenderList, selectedIndex, senderName);
							SetDlgItemTextW(hwnd, IDC_NAME, senderName);
						} else {
							// "Auto" selected, clear custom name
							SetDlgItemTextW(hwnd, IDC_NAME, L"");
						}
					}
					break;

				default :
					break;
			}

			if (m_bIsInitialized)
			{
				m_bDirty = TRUE;
				if (m_pPageSite)
				{
					m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
				}
			}

			return (LRESULT)1;
		}

	case WM_NOTIFY:
		{
			// Tab control removed - no notification handling needed
			break;
		}
	}

	// Let the parent class handle the message.
	return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

//OnConnect is called when the client creates the property page. It sets the IUnknown pointer to the filter.
HRESULT CSpoutCamProperties::OnConnect(IUnknown *pUnknown)
{
	TRACE("OnConnect");
	OutputDebugStringA("CSpoutCamProperties: OnConnect called");
	CheckPointer(pUnknown, E_POINTER);
	ASSERT(m_pCamSettings == NULL);

	HRESULT hr = pUnknown->QueryInterface(IID_ICamSettings, (void **)&m_pCamSettings);
	if (FAILED(hr))
	{
		OutputDebugStringA("CSpoutCamProperties: OnConnect - QueryInterface failed");
		return E_NOINTERFACE;
	}

	// Get the initial image FX property
	CheckPointer(m_pCamSettings, E_FAIL);

	// If camera name was set by factory, use that to determine camera info
	if (!m_cameraName.empty()) {
		// Get camera config from DynamicCameraManager
		auto manager = SpoutCam::DynamicCameraManager::GetInstance();
		auto cameraConfig = manager->GetCamera(m_cameraName);
		if (cameraConfig) {
			// Find the index for backward compatibility
			auto allCameras = manager->GetAllCameras();
			for (int i = 0; i < (int)allCameras.size(); i++) {
				if (allCameras[i] == cameraConfig) {
					m_cameraIndex = i;
					break;
				}
			}
			char debugMsg[256];
			sprintf_s(debugMsg, "CSpoutCamProperties: OnConnect - Using camera name '%s' with index %d", m_cameraName.c_str(), m_cameraIndex);
			OutputDebugStringA(debugMsg);
		} else {
			OutputDebugStringA("CSpoutCamProperties: OnConnect - Camera name set but not found in manager");
		}
	} else {
		// Fall back to old method - get camera index from filter instance
		hr = m_pCamSettings->get_CameraIndex(&m_cameraIndex);
		if (FAILED(hr))
		{
			m_cameraIndex = 0; // Default to camera 0 if we can't get the index
			OutputDebugStringA("CSpoutCamProperties: OnConnect - Failed to get camera index, using 0");
		} else {
			// Try to get camera name from the filter
			char cameraName[256] = {0};
			if (SUCCEEDED(m_pCamSettings->get_CameraName(cameraName, sizeof(cameraName)))) {
				m_cameraName = cameraName;
				char debugMsg[256];
				sprintf_s(debugMsg, "CSpoutCamProperties: OnConnect - Got camera index %d and name '%s'", m_cameraIndex, m_cameraName.c_str());
				OutputDebugStringA(debugMsg);
			} else {
				char debugMsg[128];
				sprintf_s(debugMsg, "CSpoutCamProperties: OnConnect - Got camera index: %d", m_cameraIndex);
				OutputDebugStringA(debugMsg);
			}
		}
	}

	m_bIsInitialized = FALSE;

	return S_OK;
}

//OnDisconnect is called when the user dismisses the property sheet.
HRESULT CSpoutCamProperties::OnDisconnect()
{
	TRACE("OnDisconnect");
	// Release of Interface after setting the appropriate old effect value
	if (m_pCamSettings)
	{
		m_pCamSettings->Release();
		m_pCamSettings = NULL;
	}

	return S_OK;
}

//OnActivate is called when the dialog is created.
HRESULT CSpoutCamProperties::OnActivate()
{
	TRACE("OnActivate");

	// Window title and centering now handled in WM_INITDIALOG

	HWND hwndCtl = nullptr;
	DWORD dwValue = 0;

	////////////////////////////////////////
	// Initialize Enhanced UI Elements
	////////////////////////////////////////
	
	// Set the camera name display
	InitializeCameraName();
	
	// Populate the available senders list
	PopulateAvailableSenders();

	////////////////////////////////////////
	// Fps
	////////////////////////////////////////

	// Initialize fps dropdown with defaults (actual values loaded by InitializeProps)
	dwValue = 1; // default 1 = 30 fps

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	
	WCHAR fps_values[6][3] =
	{
		L"10",
		L"15",
		L"25",
		L"30", // default
		L"50",
		L"60"
	};

	WCHAR fps[3];
	int k = 0;

	memset(&fps, 0, sizeof(fps));
	for (k = 0; k <= 5; k += 1)
	{
		wcscpy_s(fps, sizeof(fps) / sizeof(WCHAR), fps_values[k]);
		ComboBox_AddString(hwndCtl, fps);
	}

	// Select default
	ComboBox_SetCurSel(hwndCtl, dwValue);

	////////////////////////////////////////
	// Resolution
	////////////////////////////////////////

	// Initialize resolution dropdown with defaults (actual values loaded by InitializeProps)
	dwValue = 0; // default 0 = active sender

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_RESOLUTION);

	WCHAR res_values[11][14] =
	{
		L"Active Sender",
		L"320 x 240",
		L"640 x 360",
		L"640 x 480", // default if no sender
		L"800 x 600",
		L"1024 x 720",
		L"1024 x 768",
		L"1280 x 720",
		L"1280 x 960",
		L"1280 x 1024",
		L"1920 x 1080"
	};

	WCHAR res[14];
	memset(&res, 0, sizeof(res));
	for (k = 0; k <= 10; k += 1)
	{
		wcscpy_s(res, sizeof(res) / sizeof(WCHAR), res_values[k]);
		ComboBox_AddString(hwndCtl, res);
	}

	// Select current value
	ComboBox_SetCurSel(hwndCtl, dwValue);

	////////////////////////////////////////
	// Camera Name
	////////////////////////////////////////
	
	// Initialize camera name edit field (actual value loaded by InitializeProps)
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_CAMERA_NAME_EDIT);
	Edit_SetText(hwndCtl, L"");

	////////////////////////////////////////
	// Starting sender name
	////////////////////////////////////////

	// Initialize sender name (actual values loaded by InitializeProps)
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_SetText(hwndCtl, L"");

	////////////////////////////////////////
	// Mirror, Swap, Flip - Initialize with defaults (actual values loaded by InitializeProps)
	////////////////////////////////////////
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	Button_SetCheck(hwndCtl, 0);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	Button_SetCheck(hwndCtl, 0);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	Button_SetCheck(hwndCtl, 0);

	// Warning disable mode
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "silent", &dwValue))
	{
		dwValue = 0; // Disable warnings off by default
	}
	m_bSilent = (dwValue > 0);
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SILENT);
	Button_SetCheck(hwndCtl, dwValue);

	// Show SpoutCam version
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_VERS);
	Static_SetText(hwndCtl, L"Version: " _VER_VERSION_STRING);

	// Initialize properties dialog
	InitializeProps();

	m_bIsInitialized = TRUE;

	if (m_pPageSite)
	{
		m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
	}

	return S_OK;
}

//OnDeActivate is called when the dialog is closed.
HRESULT CSpoutCamProperties::OnDeactivate()
{
	if (g_hbrBkgnd)
		DeleteObject(g_hbrBkgnd);
	return S_OK;
}

//OnApplyChanges is called when the user commits the property changes by clicking the OK or Apply button.
HRESULT CSpoutCamProperties::OnApplyChanges()
{
	TRACE("OnApplyChanges");

	DWORD dwFps, dwResolution, dwMirror, dwSwap, dwFlip, dwSilent;
	wchar_t wname[256];
	char name[256];
	char keyName[256];

	// Create camera-specific registry key using camera name
	GetCameraRegistryPath(keyName, sizeof(keyName));

	// =================================
	// Get old fps and resolution for user warning
	DWORD dwOldFps, dwOldResolution;
	ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &dwOldFps);
	ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &dwOldResolution);
	dwFps = ComboBox_GetCurSel(GetDlgItem(this->m_Dlg, IDC_FPS));
	dwResolution = ComboBox_GetCurSel(GetDlgItem(this->m_Dlg, IDC_RESOLUTION));
	// Warn unless disabled (only for fps/resolution changes that require restart)
	bool needsReregistration = false;
	if (!m_bSilent) {
		if (dwOldFps != dwFps || dwOldResolution != dwResolution) {
			wchar_t warningMsg[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(warningMsg, L"Resolution or FPS change on %s requires reregistration.\n\nClick OK to automatically reregister the camera with new settings.\nClick Cancel to revert changes.", !m_cameraName.empty() ? cameraDisplayName : L"this camera");
			if (MessageBoxW(NULL, warningMsg, L"Camera Reregistration Required", MB_OKCANCEL | MB_TOPMOST | MB_ICONQUESTION) == IDCANCEL) {
				if (dwOldFps != dwFps)
					ComboBox_SetCurSel(GetDlgItem(this->m_Dlg, IDC_FPS), dwOldFps);
				if (dwOldResolution != dwResolution)
					ComboBox_SetCurSel(GetDlgItem(this->m_Dlg, IDC_RESOLUTION), dwOldResolution);
				return -1;
			}
			needsReregistration = true;
		}
	} else if (dwOldFps != dwFps || dwOldResolution != dwResolution) {
		// Silent mode - still need reregistration, just no dialog
		needsReregistration = true;
	}
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "fps", dwFps);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "resolution", dwResolution);
	// =================================

	// Camera name settings - save to dynamic camera registry
	HWND hwndCameraNameCtl = GetDlgItem(this->m_Dlg, IDC_CAMERA_NAME_EDIT);
	if (hwndCameraNameCtl) {
		wchar_t wcameraname[256];
		char cameraname[256];
		wcameraname[0] = 0;
		Edit_GetText(hwndCameraNameCtl, wcameraname, 256);
		size_t cameraNameConvertedChars = 0;
		wcstombs_s(&cameraNameConvertedChars, cameraname, 256, wcameraname, _TRUNCATE);
		
		// Update the camera name in the dynamic camera registry if it has changed
		if (strlen(cameraname) > 0) {
			UpdateCameraName(cameraname);
		}
	}

	// Sender name settings
	HWND hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	wname[0] = 0;
	Edit_GetText(hwndCtl, wname, 256);
	size_t convertedChars = 0;
	wcstombs_s(&convertedChars, name, 256, wname, _TRUNCATE);
	WritePathToRegistry(HKEY_CURRENT_USER, keyName, "senderstart", name);

	// Mirror, Swap, Flip settings - these should apply immediately
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	dwMirror = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "mirror", dwMirror);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	dwSwap = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "swap", dwSwap);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	dwFlip = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "flip", dwFlip);

	// Silent mode is global, not per-camera
	dwSilent = Button_GetCheck(GetDlgItem(this->m_Dlg, IDC_SILENT));
	WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "silent", dwSilent);
	
	// Apply settings to the camera filter for immediate effect
	if (m_pCamSettings)
		m_pCamSettings->put_Settings(dwFps, dwResolution, dwMirror, dwSwap, dwFlip, name);

	// Perform automatic reregistration if needed for FPS/resolution changes
	if (needsReregistration) {
		AutoReregisterCamera();
	}

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// Enhanced UI Methods for Multiple Camera Support
//////////////////////////////////////////////////////////////////////////

// Helper to determine which camera this properties dialog is for
HRESULT CSpoutCamProperties::GetCameraIndex(int* pCameraIndex)
{
	if (!pCameraIndex) return E_POINTER;
	
	// Return the camera index for this specific properties dialog instance
	*pCameraIndex = m_cameraIndex;
	
	return S_OK;
}

// Set the correct registry path based on camera index (legacy method)
void CSpoutCamProperties::SetRegistryPath(char* registryPath, int cameraIndex)
{
	if (cameraIndex == 0) {
		strcpy_s(registryPath, 256, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(registryPath, 256, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
	}
}

// Get the correct registry path based on camera name (preferred method)
void CSpoutCamProperties::GetCameraRegistryPath(char* registryPath, size_t bufferSize)
{
	if (!m_cameraName.empty()) {
		sprintf_s(registryPath, bufferSize, "Software\\Leading Edge\\SpoutCam\\%s", m_cameraName.c_str());
	} else {
		// Fallback to old logic
		if (m_cameraIndex == 0) {
			strcpy_s(registryPath, bufferSize, "Software\\Leading Edge\\SpoutCam");
		} else {
			sprintf_s(registryPath, bufferSize, "Software\\Leading Edge\\SpoutCam%d", m_cameraIndex + 1);
		}
	}
}

// Override to provide dynamic tab title
STDMETHODIMP CSpoutCamProperties::GetPageInfo(LPPROPPAGEINFO pPageInfo)
{
	CheckPointer(pPageInfo, E_POINTER);
	
	// Get the camera index directly from the filter if available
	int actualCameraIndex = m_cameraIndex; // Default to member variable
	if (m_pCamSettings) {
		HRESULT hr = m_pCamSettings->get_CameraIndex(&actualCameraIndex);
		if (FAILED(hr)) {
			actualCameraIndex = m_cameraIndex; // Fall back to member variable
		}
	}
	
	// Debug output to see what camera index we're using
	char debugMsg[128];
	sprintf_s(debugMsg, "GetPageInfo: m_cameraIndex = %d, actualCameraIndex = %d", m_cameraIndex, actualCameraIndex);
	OutputDebugStringA(debugMsg);
	
	// Set tab title to "Settings"
	WCHAR cameraName[64];
	wcscpy_s(cameraName, 64, L"Settings");
	
	// More debug output
	char debugMsg2[128];
	sprintf_s(debugMsg2, "GetPageInfo: Setting tab title to: %S", cameraName);
	OutputDebugStringA(debugMsg2);
	
	// Allocate memory for the title and copy it
	LPOLESTR pszTitle = (LPOLESTR)CoTaskMemAlloc(wcslen(cameraName) * sizeof(WCHAR) + sizeof(WCHAR));
	if (pszTitle == NULL) {
		return E_OUTOFMEMORY;
	}
	wcscpy_s(pszTitle, wcslen(cameraName) + 1, cameraName);
	
	// Fill in the page info
	pPageInfo->cb = sizeof(PROPPAGEINFO);
	pPageInfo->pszTitle = pszTitle;
	pPageInfo->size.cx = 250;
	pPageInfo->size.cy = 440;
	pPageInfo->pszDocString = NULL;
	pPageInfo->pszHelpFile = NULL;
	pPageInfo->dwHelpContext = 0;
	
	return S_OK;
}

// Initialize the camera name display
void CSpoutCamProperties::InitializeCameraName()
{
	// Update the camera name label in the dialog to show the specific camera
	HWND hwndName = GetDlgItem(this->m_Dlg, IDC_CAMERA_NAME);
	if (hwndName) {
		WCHAR cameraName[256];
		
		// Try to read custom camera name from registry
		bool foundCustomName = false;
		
		// Check if there's a selected camera index and name in registry
		DWORD selectedCameraIndex = 0;
		char customName[256] = "";
		
		if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "SelectedCameraIndex", &selectedCameraIndex) &&
			selectedCameraIndex == m_cameraIndex &&
			ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutCam", "SelectedCameraName", customName)) {
			
			if (strlen(customName) > 0) {
				MultiByteToWideChar(CP_ACP, 0, customName, -1, cameraName, 256);
				foundCustomName = true;
			}
		}
		
		// Fallback to default naming if no custom name found
		if (!foundCustomName) {
			if (m_cameraIndex == 0) {
				wcscpy_s(cameraName, 256, L"SpoutCam");
			} else {
				swprintf_s(cameraName, 256, L"SpoutCam%d", m_cameraIndex + 1);
			}
		}
		
		Static_SetText(hwndName, cameraName);
	}
}

// Populate the sender list with available Spout senders
void CSpoutCamProperties::PopulateAvailableSenders()
{
	HWND hwndSenderList = GetDlgItem(this->m_Dlg, IDC_SENDER_LIST);
	if (!hwndSenderList) return;
	
	// Clear existing items
	ComboBox_ResetContent(hwndSenderList);
	
	// Add "Auto" option (use active sender)
	ComboBox_AddString(hwndSenderList, L"Auto (Active Sender)");
	
	// Create a temporary SpoutDX receiver instance for enumeration
	spoutDX spoutReceiver;
	
	// Get the number of available senders
	int senderCount = spoutReceiver.GetSenderCount();
	
	if (senderCount > 0) {
		// Enumerate all available senders
		for (int i = 0; i < senderCount; i++) {
			char senderName[256] = {0};
			if (spoutReceiver.GetSender(i, senderName, 256)) {
				// Convert to wide string for the combo box
				WCHAR wideSenderName[256];
				MultiByteToWideChar(CP_ACP, 0, senderName, -1, wideSenderName, 256);
				ComboBox_AddString(hwndSenderList, wideSenderName);
			}
		}
	}
	
	// Select "Auto" by default
	ComboBox_SetCurSel(hwndSenderList, 0);
}

// Refresh the sender list
void CSpoutCamProperties::RefreshSenderList()
{
	PopulateAvailableSenders();
}

// External function from dll.cpp
extern "C" STDAPI RegisterFilters(BOOL bRegister);

// Register all SpoutCam cameras
void CSpoutCamProperties::RegisterCameras()
{
	HRESULT hr = RegisterFilters(TRUE);
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			MessageBox(m_Dlg, L"SpoutCam cameras registered successfully!", L"Registration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		MessageBox(m_Dlg, L"Failed to register SpoutCam cameras.\n\nThis may require administrator privileges.", L"Registration Error", MB_OK | MB_ICONERROR);
	}
}

// Unregister all SpoutCam cameras - simplified without UAC elevation for now
void CSpoutCamProperties::UnregisterCameras()
{
	HRESULT hr = RegisterFilters(FALSE);
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			MessageBox(m_Dlg, L"SpoutCam cameras unregistered successfully!", L"Unregistration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		MessageBox(m_Dlg, L"Failed to unregister SpoutCam cameras.\n\nThis may require administrator privileges.", L"Unregistration Error", MB_OK | MB_ICONERROR);
	}
}

// Register single camera using camera name (no index needed)
void CSpoutCamProperties::RegisterSingleCamera(int cameraIndex)
{
	// Use the current camera name instead of index
	HRESULT hr = RegisterCameraByName(m_cameraName.c_str());
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[256];
			wchar_t wideCameraName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wideCameraName, 256);
			swprintf_s(message, L"%s registered successfully!", wideCameraName);
			MessageBox(m_Dlg, message, L"Registration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[256];
		wchar_t wideCameraName[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wideCameraName, 256);
		swprintf_s(message, L"Failed to register %s.\n\nThis may require administrator privileges.", wideCameraName);
		MessageBox(m_Dlg, message, L"Registration Error", MB_OK | MB_ICONERROR);
	}
}

// Unregister single camera using camera name (no index needed)
void CSpoutCamProperties::UnregisterSingleCamera(int cameraIndex)
{
	// Use the current camera name instead of index  
	HRESULT hr = UnregisterCameraByName(m_cameraName.c_str());
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[256];
			wchar_t wideCameraName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wideCameraName, 256);
			swprintf_s(message, L"%s unregistered successfully!", wideCameraName);
			MessageBox(m_Dlg, message, L"Unregistration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[256];
		wchar_t wideCameraName[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wideCameraName, 256);
		swprintf_s(message, L"Failed to unregister %s.\n\nThis may require administrator privileges.", wideCameraName);
		MessageBox(m_Dlg, message, L"Unregistration Error", MB_OK | MB_ICONERROR);
	}
}

// Register current camera using camera name
void CSpoutCamProperties::RegisterCurrentCamera()
{
	if (m_cameraName.empty()) {
		MessageBox(m_Dlg, L"Cannot register camera: camera name not available.", L"Registration Error", MB_OK | MB_ICONERROR);
		return;
	}

	HRESULT hr = RegisterCameraByName(m_cameraName.c_str());
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(message, L"%s registered successfully!", cameraDisplayName);
			MessageBox(m_Dlg, message, L"Registration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[512];
		wchar_t cameraDisplayName[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
		swprintf_s(message, L"Failed to register %s.\n\nThis may require administrator privileges.", cameraDisplayName);
		MessageBox(m_Dlg, message, L"Registration Error", MB_OK | MB_ICONERROR);
	}
}

// Unregister current camera using camera name
void CSpoutCamProperties::UnregisterCurrentCamera()
{
	if (m_cameraName.empty()) {
		MessageBox(m_Dlg, L"Cannot unregister camera: camera name not available.", L"Unregistration Error", MB_OK | MB_ICONERROR);
		return;
	}

	HRESULT hr = UnregisterCameraByName(m_cameraName.c_str());
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(message, L"%s unregistered successfully!", cameraDisplayName);
			MessageBox(m_Dlg, message, L"Unregistration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[512];
		wchar_t cameraDisplayName[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
		swprintf_s(message, L"Failed to unregister %s.\n\nThis may require administrator privileges.", cameraDisplayName);
		MessageBox(m_Dlg, message, L"Unregistration Error", MB_OK | MB_ICONERROR);
	}
}

// Legacy tab functions removed (now handled by InitializeProps)

// Add a new camera tab
void CSpoutCamProperties::AddCameraTab()
{
	// Legacy function - tabs removed from properties dialog
	// Functionality moved to SpoutCamSettings manager UI
}

// Remove the current camera tab
void CSpoutCamProperties::RemoveCameraTab()
{
	// Legacy function - tabs removed from properties dialog
	// Functionality moved to SpoutCamSettings manager UI
}

// Handle tab selection change
void CSpoutCamProperties::OnTabSelectionChange()
{
	// Legacy function - tabs removed from properties dialog
	// No tab switching needed
}

// Update the display for the current camera
void CSpoutCamProperties::UpdateCameraDisplay()
{
	// Load settings for the current camera (LoadCameraSettings handles camera name lookup)
	LoadCameraSettings(0); // Index parameter is ignored, function uses camera name internally
	
	// Force refresh of all controls to fix rendering issues
	RefreshControlsDisplay();
}

// Save current camera settings (per-tab settings)
void CSpoutCamProperties::SaveCurrentCameraSettings()
{
	// Save current control values to registry with camera-specific keys
	DWORD dwFps, dwResolution, dwMirror, dwSwap, dwFlip;
	wchar_t wname[256];
	char name[256];
	char keyName[256];

	// Create camera-specific registry key using camera name
	GetCameraRegistryPath(keyName, sizeof(keyName));

	HWND hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	dwFps = ComboBox_GetCurSel(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "fps", dwFps);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_RESOLUTION);
	dwResolution = ComboBox_GetCurSel(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "resolution", dwResolution);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_GetText(hwndCtl, wname, 256);
	WideCharToMultiByte(CP_ACP, 0, wname, -1, name, 256, NULL, NULL);
	WritePathToRegistry(HKEY_CURRENT_USER, keyName, "senderstart", name);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	dwMirror = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "mirror", dwMirror);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	dwSwap = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "swap", dwSwap);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	dwFlip = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "flip", dwFlip);
}

// Load settings for a specific camera tab
void CSpoutCamProperties::LoadCameraSettings(int cameraIndex)
{
	DWORD dwValue;
	char name[256];
	wchar_t wname[256];
	HWND hwndCtl;
	char keyName[256];

	// Load camera name from dynamic camera registry
	LoadCameraName();

	// Create camera-specific registry key using camera name
	GetCameraRegistryPath(keyName, sizeof(keyName));

	// Load FPS - use sender-based defaults if no saved settings
	DWORD defaultFps, defaultResolution;
	GetActiveSenderDefaults(&defaultFps, &defaultResolution);
	
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &dwValue) || dwValue > 5) {
		dwValue = defaultFps; // Use sender-based default
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	ComboBox_SetCurSel(hwndCtl, dwValue);

	// Load Resolution - use sender-based defaults if no saved settings
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &dwValue) || dwValue > 10) {
		dwValue = defaultResolution; // Use sender-based default
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_RESOLUTION);
	ComboBox_SetCurSel(hwndCtl, dwValue);

	// Load sender name
	if (!ReadPathFromRegistry(HKEY_CURRENT_USER, keyName, "senderstart", name)) {
		name[0] = 0;
	}
	if (name[0] != 0) {
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, wname, 256, name, 256);
	} else {
		wname[0] = 0;
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_SetText(hwndCtl, wname);

	// Load mirror
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &dwValue)) {
		dwValue = 0;
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	Button_SetCheck(hwndCtl, dwValue);

	// Load swap
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "swap", &dwValue)) {
		dwValue = 0;
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	Button_SetCheck(hwndCtl, dwValue);

	// Load flip
	if (!ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "flip", &dwValue)) {
		dwValue = 0;
	}
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	Button_SetCheck(hwndCtl, dwValue);

	// Refresh sender list for this camera
	RefreshSenderList();
}

// Apply real-time settings changes (mirror, flip, swap) immediately
void CSpoutCamProperties::ApplyRealTimeSettings()
{
	DWORD dwMirror, dwSwap, dwFlip;
	char keyName[256];
	char name[256];
	wchar_t wname[256];

	// Create camera-specific registry key using camera name
	GetCameraRegistryPath(keyName, sizeof(keyName));

	// Get current control values
	HWND hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	dwMirror = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "mirror", dwMirror);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	dwSwap = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "swap", dwSwap);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	dwFlip = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, keyName, "flip", dwFlip);

	// Get current fps, resolution, and sender name for complete settings update
	DWORD dwFps = ComboBox_GetCurSel(GetDlgItem(this->m_Dlg, IDC_FPS));
	DWORD dwResolution = ComboBox_GetCurSel(GetDlgItem(this->m_Dlg, IDC_RESOLUTION));
	
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_GetText(hwndCtl, wname, 256);
	WideCharToMultiByte(CP_ACP, 0, wname, -1, name, 256, NULL, NULL);

	// Apply settings to the camera filter for immediate effect
	if (m_pCamSettings) {
		m_pCamSettings->put_Settings(dwFps, dwResolution, dwMirror, dwSwap, dwFlip, name);
	}
}

// Refresh all controls display to fix rendering issues when switching tabs
void CSpoutCamProperties::RefreshControlsDisplay()
{
	// List of all control IDs that need refreshing
	int controlIds[] = {
		IDC_FPS, IDC_RESOLUTION, IDC_SENDER_LIST, IDC_REFRESH,
		IDC_NAME, IDC_MIRROR, IDC_FLIP, IDC_SWAP,
		IDC_REGISTER, IDC_UNREGISTER, IDC_REREGISTER, IDC_REMOVE_CAMERA
	};

	// Force redraw of all controls
	for (int i = 0; i < sizeof(controlIds) / sizeof(int); i++) {
		HWND hwndCtl = GetDlgItem(this->m_Dlg, controlIds[i]);
		if (hwndCtl) {
			InvalidateRect(hwndCtl, NULL, TRUE);
			UpdateWindow(hwndCtl);
		}
	}

	// Also refresh the dialog itself
	InvalidateRect(this->m_Dlg, NULL, TRUE);
	UpdateWindow(this->m_Dlg);
}

// Automatically reregister the current camera after FPS/resolution changes
void CSpoutCamProperties::AutoReregisterCamera()
{
	if (m_cameraName.empty()) {
		if (!m_bSilent) {
			MessageBox(m_Dlg, L"Cannot reregister camera: camera name not available.", L"Reregistration Error", MB_OK | MB_ICONERROR);
		}
		return;
	}

	// First unregister the current camera
	HRESULT hr = UnregisterCameraByName(m_cameraName.c_str());
	if (FAILED(hr)) {
		if (!m_bSilent) {
			wchar_t errorMsg[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(errorMsg, L"Failed to unregister %s during reregistration.\nYou may need to manually unregister and register the camera.", cameraDisplayName);
			MessageBox(m_Dlg, errorMsg, L"Reregistration Warning", MB_OK | MB_ICONWARNING);
		}
		return;
	}

	// Small delay to ensure clean unregistration
	Sleep(100);

	// Re-register the camera with new settings
	hr = RegisterCameraByName(m_cameraName.c_str());
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t successMsg[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(successMsg, L"%s has been successfully reregistered with new settings.\nThe camera is now ready to use with updated resolution/FPS.", cameraDisplayName);
			MessageBox(m_Dlg, successMsg, L"Reregistration Complete", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		if (!m_bSilent) {
			wchar_t errorMsg[512];
			wchar_t cameraDisplayName[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, cameraDisplayName, 256);
			swprintf_s(errorMsg, L"Failed to reregister %s.\nPlease manually register the camera using the 'Register This Camera' button.", cameraDisplayName);
			MessageBox(m_Dlg, errorMsg, L"Reregistration Error", MB_OK | MB_ICONERROR);
		}
	}
}

// Get default FPS and resolution based on active sender properties
void CSpoutCamProperties::GetActiveSenderDefaults(DWORD* pDefaultFps, DWORD* pDefaultResolution)
{
	if (!pDefaultFps || !pDefaultResolution) return;
	
	// Set fallback defaults
	*pDefaultFps = 3; // 30 FPS
	*pDefaultResolution = 0; // Active sender
	
	// Create a temporary SpoutDX receiver instance to query active sender
	spoutDX spoutReceiver;
	char senderName[256];
	unsigned int senderWidth = 0, senderHeight = 0;
	
	// Try to get active sender information
	if (spoutReceiver.GetActiveSender(senderName)) {
		HANDLE dxShareHandle = NULL;
		DWORD dwFormat = 0;
		if (spoutReceiver.GetSenderInfo(senderName, senderWidth, senderHeight, dxShareHandle, dwFormat)) {
			// Map sender resolution to our resolution index
			if (senderWidth == 320 && senderHeight == 240) *pDefaultResolution = 1;
			else if (senderWidth == 640 && senderHeight == 360) *pDefaultResolution = 2;
			else if (senderWidth == 640 && senderHeight == 480) *pDefaultResolution = 3;
			else if (senderWidth == 800 && senderHeight == 600) *pDefaultResolution = 4;
			else if (senderWidth == 1024 && senderHeight == 720) *pDefaultResolution = 5;
			else if (senderWidth == 1024 && senderHeight == 768) *pDefaultResolution = 6;
			else if (senderWidth == 1280 && senderHeight == 720) *pDefaultResolution = 7;
			else if (senderWidth == 1280 && senderHeight == 960) *pDefaultResolution = 8;
			else if (senderWidth == 1280 && senderHeight == 1024) *pDefaultResolution = 9;
			else if (senderWidth == 1920 && senderHeight == 1080) *pDefaultResolution = 10;
			else *pDefaultResolution = 0; // Use active sender size
			
			// For FPS, we could estimate based on resolution:
			// Higher resolutions typically need lower FPS for smooth performance
			if (senderWidth >= 1920 && senderHeight >= 1080) {
				*pDefaultFps = 3; // 30 FPS for 1080p+
			} else if (senderWidth >= 1280 && senderHeight >= 720) {
				*pDefaultFps = 3; // 30 FPS for 720p+
			} else {
				*pDefaultFps = 5; // 60 FPS for smaller resolutions
			}
		}
	}
}

// Load the camera name from the dynamic camera registry
void CSpoutCamProperties::LoadCameraName()
{
	HWND hwndCameraNameCtl = GetDlgItem(this->m_Dlg, IDC_CAMERA_NAME_EDIT);
	if (!hwndCameraNameCtl) return;

	// If we already have the camera name from the factory or OnConnect, use it directly
	if (!m_cameraName.empty()) {
		wchar_t wcameraname[256];
		MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wcameraname, 256);
		Edit_SetText(hwndCameraNameCtl, wcameraname);
		return;
	}

	// If no camera name is set, try to get it from the DynamicCameraManager using the camera index
	auto manager = SpoutCam::DynamicCameraManager::GetInstance();
	auto allCameras = manager->GetAllCameras();
	
	if (m_cameraIndex >= 0 && m_cameraIndex < (int)allCameras.size()) {
		auto cameraConfig = allCameras[m_cameraIndex];
		if (cameraConfig) {
			m_cameraName = cameraConfig->name;
			wchar_t wcameraname[256];
			MultiByteToWideChar(CP_ACP, 0, m_cameraName.c_str(), -1, wcameraname, 256);
			Edit_SetText(hwndCameraNameCtl, wcameraname);
			return;
		}
	}
	
	// Final fallback: use default name based on camera index
	char defaultName[64];
	if (m_cameraIndex == 0) {
		strcpy_s(defaultName, "SpoutCam");
	} else {
		sprintf_s(defaultName, "SpoutCam%d", m_cameraIndex + 1);
	}
	
	m_cameraName = defaultName;
	wchar_t wcameraname[256];
	MultiByteToWideChar(CP_ACP, 0, defaultName, -1, wcameraname, 256);
	Edit_SetText(hwndCameraNameCtl, wcameraname);
}

// Update camera name in the dynamic camera registry
void CSpoutCamProperties::UpdateCameraName(const char* newName)
{
	if (!newName || strlen(newName) == 0 || m_cameraName.empty()) return;
	
	// Don't do anything if name hasn't changed
	if (m_cameraName == newName) return;
	
	auto manager = SpoutCam::DynamicCameraManager::GetInstance();
	
	// Check if new name already exists
	auto existingCamera = manager->GetCamera(newName);
	if (existingCamera) {
		// Name already exists, show error
		wchar_t errorMsg[256];
		wchar_t newNameWide[256];
		MultiByteToWideChar(CP_ACP, 0, newName, -1, newNameWide, 256);
		swprintf_s(errorMsg, L"Camera name '%s' already exists. Please choose a different name.", newNameWide);
		MessageBox(m_Dlg, errorMsg, L"Name Conflict", MB_OK | MB_ICONWARNING);
		return;
	}
	
	// Get the current camera settings from registry BEFORE renaming
	char oldRegistryPath[512];
	char newRegistryPath[512];
	GetCameraRegistryPath(oldRegistryPath, sizeof(oldRegistryPath));
	sprintf_s(newRegistryPath, sizeof(newRegistryPath), "Software\\Leading Edge\\SpoutCam\\%s", newName);
	
	// Migrate all settings from old registry path to new registry path
	HKEY oldKey, newKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, oldRegistryPath, 0, KEY_READ, &oldKey) == ERROR_SUCCESS) {
		if (RegCreateKeyExA(HKEY_CURRENT_USER, newRegistryPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &newKey, nullptr) == ERROR_SUCCESS) {
			// Copy all values from old registry location to new one
			DWORD valueCount = 0;
			DWORD maxValueNameLen = 0;
			DWORD maxValueLen = 0;
			
			if (RegQueryInfoKeyA(oldKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &valueCount, &maxValueNameLen, &maxValueLen, nullptr, nullptr) == ERROR_SUCCESS) {
				char* valueName = new char[maxValueNameLen + 1];
				BYTE* valueData = new BYTE[maxValueLen];
				
				for (DWORD i = 0; i < valueCount; i++) {
					DWORD valueNameLen = maxValueNameLen + 1;
					DWORD valueDataLen = maxValueLen;
					DWORD valueType;
					
					if (RegEnumValueA(oldKey, i, valueName, &valueNameLen, nullptr, &valueType, valueData, &valueDataLen) == ERROR_SUCCESS) {
						RegSetValueExA(newKey, valueName, 0, valueType, valueData, valueDataLen);
					}
				}
				
				delete[] valueName;
				delete[] valueData;
			}
			
			RegCloseKey(newKey);
		}
		RegCloseKey(oldKey);
		
		// Delete the old registry path
		RegDeleteTreeA(HKEY_CURRENT_USER, oldRegistryPath);
	}
	
	// Update the camera name in DynamicCameraManager (rename in-place)
	auto currentCamera = manager->GetCamera(m_cameraName);
	if (currentCamera) {
		// Rename the existing camera in-place instead of creating new one
		std::string oldName = currentCamera->name;
		currentCamera->name = newName;
		
		// Update the internal mappings in the manager
		manager->UpdateCameraName(oldName, newName);
		
		// Save the renamed camera to registry under new name
		manager->SaveCameraToRegistry(*currentCamera);
		
		// Update our current camera name
		m_cameraName = newName;
	}
}
