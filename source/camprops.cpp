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

// Tab control management
static int g_currentCameraTab = 0;
static int g_maxCameraCount = 8;  // Support up to 8 camera instances
static int g_activeCameras = 1;   // Start with 1 camera tab

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

// Constructor
CSpoutCamProperties::CSpoutCamProperties(LPUNKNOWN pUnk, HRESULT *phr) :
	CBasePropertyPage("SpoutCam Configuration", pUnk, IDD_SPOUTCAMBOX, IDS_TITLE),
	m_pCamSettings(NULL),
	m_bIsInitialized(FALSE),
	m_cameraIndex(0) // Default to camera 0, will be updated in OnConnect
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
			LPNMHDR lpnmhdr = (LPNMHDR)lParam;
			if (lpnmhdr->idFrom == IDC_CAMERA_TABS && lpnmhdr->code == TCN_SELCHANGE) {
				// Tab selection changed
				OnTabSelectionChange();
				return (LRESULT)1;
			}
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

	// Get the camera index from the filter instance
	hr = m_pCamSettings->get_CameraIndex(&m_cameraIndex);
	if (FAILED(hr))
	{
		m_cameraIndex = 0; // Default to camera 0 if we can't get the index
		OutputDebugStringA("CSpoutCamProperties: OnConnect - Failed to get camera index, using 0");
	} else {
		char debugMsg[128];
		sprintf_s(debugMsg, "CSpoutCamProperties: OnConnect - Got camera index: %d", m_cameraIndex);
		OutputDebugStringA(debugMsg);
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

	// Initialize fps dropdown with defaults (actual values loaded per-camera by InitializeTabControl)
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

	// Initialize resolution dropdown with defaults (actual values loaded per-camera by InitializeTabControl)
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
	// Starting sender name
	////////////////////////////////////////

	// Initialize sender name (actual values loaded per-camera by InitializeTabControl)
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_SetText(hwndCtl, L"");

	////////////////////////////////////////
	// Mirror, Swap, Flip - Initialize with defaults (actual values loaded per-camera by InitializeTabControl)
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

	// Initialize tab control
	InitializeTabControl();

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

	// Create camera-specific registry key for current camera (match cam.cpp logic)
	if (m_cameraIndex == 0) {
		strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", m_cameraIndex + 1);
	}

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
			wchar_t warningMsg[256];
			swprintf_s(warningMsg, L"Resolution or FPS change on SpoutCam%d requires reregistration.\n\nClick OK to automatically reregister the camera with new settings.\nClick Cancel to revert changes.", m_cameraIndex + 1);
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

// Set the correct registry path based on camera index
void CSpoutCamProperties::SetRegistryPath(char* registryPath, int cameraIndex)
{
	if (cameraIndex == 0) {
		strcpy_s(registryPath, 256, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(registryPath, 256, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);
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
		
		// Try to read custom camera name from info file
		bool foundCustomName = false;
		char exePath[MAX_PATH];
		GetModuleFileNameA(NULL, exePath, MAX_PATH);
		char* lastSlash = strrchr(exePath, '\\');
		if (lastSlash) *lastSlash = '\0';
		
		char tempFilePath[MAX_PATH];
		sprintf_s(tempFilePath, "%s\\camera_info.tmp", exePath);
		
		FILE* infoFile = nullptr;
		if (fopen_s(&infoFile, tempFilePath, "r") == 0 && infoFile) {
			char line[512];
			int fileIndex = -1;
			char customName[256] = "";
			
			while (fgets(line, sizeof(line), infoFile)) {
				// Remove newline
				line[strcspn(line, "\r\n")] = 0;
				
				if (strncmp(line, "cameraIndex=", 12) == 0) {
					fileIndex = atoi(line + 12);
				} else if (strncmp(line, "cameraName=", 11) == 0) {
					strcpy_s(customName, line + 11);
				}
			}
			fclose(infoFile);
			
			// If this file matches our camera index, use the custom name
			if (fileIndex == m_cameraIndex && strlen(customName) > 0) {
				MultiByteToWideChar(CP_ACP, 0, customName, -1, cameraName, 256);
				foundCustomName = true;
			}
			
			// Clean up the temporary file after reading
			DeleteFileA(tempFilePath);
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

// Register single camera - simplified without UAC elevation for now  
void CSpoutCamProperties::RegisterSingleCamera(int cameraIndex)
{
	HRESULT hr = RegisterSingleCameraFilter(TRUE, cameraIndex);
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[256];
			swprintf_s(message, L"SpoutCam%d registered successfully!", cameraIndex + 1);
			MessageBox(m_Dlg, message, L"Registration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[256];
		swprintf_s(message, L"Failed to register SpoutCam%d.\n\nThis may require administrator privileges.", cameraIndex + 1);
		MessageBox(m_Dlg, message, L"Registration Error", MB_OK | MB_ICONERROR);
	}
}

// Unregister single camera - simplified without UAC elevation for now
void CSpoutCamProperties::UnregisterSingleCamera(int cameraIndex)
{
	HRESULT hr = RegisterSingleCameraFilter(FALSE, cameraIndex);
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t message[256];
			swprintf_s(message, L"SpoutCam%d unregistered successfully!", cameraIndex + 1);
			MessageBox(m_Dlg, message, L"Unregistration", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		wchar_t message[256];
		swprintf_s(message, L"Failed to unregister SpoutCam%d.\n\nThis may require administrator privileges.", cameraIndex + 1);
		MessageBox(m_Dlg, message, L"Unregistration Error", MB_OK | MB_ICONERROR);
	}
}

// Initialize the properties dialog for a single camera
void CSpoutCamProperties::InitializeTabControl()
{
	// This is now a single camera properties dialog - no tabs or multi-camera controls
	g_currentCameraTab = m_cameraIndex;
	g_activeCameras = 1;
	
	// Update the display for this camera
	UpdateCameraDisplay();
}

// Add a new camera tab
void CSpoutCamProperties::AddCameraTab()
{
	if (g_activeCameras >= g_maxCameraCount) {
		MessageBox(m_Dlg, L"Maximum number of cameras reached!", L"Add Camera", MB_OK | MB_ICONWARNING);
		return;
	}

	HWND hwndTab = GetDlgItem(this->m_Dlg, IDC_CAMERA_TABS);
	if (!hwndTab) return;

	// Add new tab
	TCITEM tie = {0};
	tie.mask = TCIF_TEXT;
	
	wchar_t tabName[32];
	swprintf_s(tabName, L"SpoutCam%d", g_activeCameras + 1);
	tie.pszText = tabName;
	
	TabCtrl_InsertItem(hwndTab, g_activeCameras, &tie);
	g_activeCameras++;
	
	// Select the new tab
	TabCtrl_SetCurSel(hwndTab, g_activeCameras - 1);
	g_currentCameraTab = g_activeCameras - 1;
	
	// Update display for new camera
	UpdateCameraDisplay();
}

// Remove the current camera tab
void CSpoutCamProperties::RemoveCameraTab()
{
	if (g_activeCameras <= 1) {
		MessageBox(m_Dlg, L"Cannot remove the last camera!", L"Remove Camera", MB_OK | MB_ICONWARNING);
		return;
	}

	HWND hwndTab = GetDlgItem(this->m_Dlg, IDC_CAMERA_TABS);
	if (!hwndTab) return;

	// Remove the current tab
	TabCtrl_DeleteItem(hwndTab, g_currentCameraTab);
	g_activeCameras--;
	
	// Adjust current tab selection
	if (g_currentCameraTab >= g_activeCameras) {
		g_currentCameraTab = g_activeCameras - 1;
	}
	
	TabCtrl_SetCurSel(hwndTab, g_currentCameraTab);
	
	// Update display for current camera
	UpdateCameraDisplay();
}

// Handle tab selection change
void CSpoutCamProperties::OnTabSelectionChange()
{
	HWND hwndTab = GetDlgItem(this->m_Dlg, IDC_CAMERA_TABS);
	if (!hwndTab) return;

	// Save current camera settings before switching
	SaveCurrentCameraSettings();
	
	// Get new selection
	g_currentCameraTab = TabCtrl_GetCurSel(hwndTab);
	
	// Update display for selected camera
	UpdateCameraDisplay();
}

// Update the display for the current camera tab
void CSpoutCamProperties::UpdateCameraDisplay()
{
	// Load settings for the current camera
	LoadCameraSettings(m_cameraIndex);
	
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

	// Create camera-specific registry key (match cam.cpp logic)
	if (m_cameraIndex == 0) {
		strcpy_s(keyName, "SpoutCam");
	} else {
		sprintf_s(keyName, "SpoutCam%d", m_cameraIndex + 1);
	}

	HWND hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	dwFps = ComboBox_GetCurSel(hwndCtl);
	sprintf_s(name, "Software\\Leading Edge\\%s", keyName);
	WriteDwordToRegistry(HKEY_CURRENT_USER, name, "fps", dwFps);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_RESOLUTION);
	dwResolution = ComboBox_GetCurSel(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, name, "resolution", dwResolution);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_NAME);
	Edit_GetText(hwndCtl, wname, 256);
	WideCharToMultiByte(CP_ACP, 0, wname, -1, name, 256, NULL, NULL);
	if (m_cameraIndex == 0) {
		strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", m_cameraIndex + 1);
	}
	WritePathToRegistry(HKEY_CURRENT_USER, keyName, "senderstart", name);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MIRROR);
	dwMirror = Button_GetCheck(hwndCtl);
	if (m_cameraIndex == 0) {
		strcpy_s(name, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(name, "Software\\Leading Edge\\SpoutCam%d", m_cameraIndex + 1);
	}
	WriteDwordToRegistry(HKEY_CURRENT_USER, name, "mirror", dwMirror);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_SWAP);
	dwSwap = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, name, "swap", dwSwap);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FLIP);
	dwFlip = Button_GetCheck(hwndCtl);
	WriteDwordToRegistry(HKEY_CURRENT_USER, name, "flip", dwFlip);
}

// Load settings for a specific camera tab
void CSpoutCamProperties::LoadCameraSettings(int cameraIndex)
{
	DWORD dwValue;
	char name[256];
	wchar_t wname[256];
	HWND hwndCtl;
	char keyName[256];

	// Create camera-specific registry key
	sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", cameraIndex + 1);

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

	// Create camera-specific registry key for current camera (match cam.cpp logic)
	if (m_cameraIndex == 0) {
		strcpy_s(keyName, "Software\\Leading Edge\\SpoutCam");
	} else {
		sprintf_s(keyName, "Software\\Leading Edge\\SpoutCam%d", m_cameraIndex + 1);
	}

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
		IDC_REGISTER, IDC_UNREGISTER, IDC_REMOVE_CAMERA
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
	// First unregister the current camera
	HRESULT hr = RegisterSingleCameraFilter(FALSE, m_cameraIndex);
	if (FAILED(hr)) {
		if (!m_bSilent) {
			wchar_t errorMsg[256];
			swprintf_s(errorMsg, L"Failed to unregister SpoutCam%d during reregistration.\nYou may need to manually unregister and register the camera.", m_cameraIndex + 1);
			MessageBox(m_Dlg, errorMsg, L"Reregistration Warning", MB_OK | MB_ICONWARNING);
		}
		return;
	}

	// Small delay to ensure clean unregistration
	Sleep(100);

	// Re-register the camera with new settings
	hr = RegisterSingleCameraFilter(TRUE, m_cameraIndex);
	if (SUCCEEDED(hr)) {
		if (!m_bSilent) {
			wchar_t successMsg[256];
			swprintf_s(successMsg, L"SpoutCam%d has been successfully reregistered with new settings.\nThe camera is now ready to use with updated resolution/FPS.", m_cameraIndex + 1);
			MessageBox(m_Dlg, successMsg, L"Reregistration Complete", MB_OK | MB_ICONINFORMATION);
		}
	} else {
		if (!m_bSilent) {
			wchar_t errorMsg[256];
			swprintf_s(errorMsg, L"Failed to reregister SpoutCam%d.\nPlease manually register the camera using the 'Register This Camera' button.", m_cameraIndex + 1);
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
