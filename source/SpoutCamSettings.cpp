//
// SpoutCamSettings.exe - Camera Management Interface
// Central interface for managing multiple SpoutCam instances
//

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include "resource.h"

// Forward declarations
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void PopulateCameraList(HWND hListView);
void RegisterCamera(int cameraIndex);
void UnregisterCamera(int cameraIndex);
void OpenCameraProperties(int cameraIndex);
bool IsCameraRegistered(int cameraIndex);
bool HasCameraSettings(int cameraIndex);
void RefreshCameraList(HWND hListView);

// Include required libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

// Registry utility function - simplified implementation  
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

#define MAX_CAMERAS 8

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Debug: Show we started
    MessageBox(nullptr, "SpoutCamSettings starting...", "Debug", MB_OK);
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icex)) {
        MessageBox(nullptr, "Failed to initialize common controls", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    MessageBox(nullptr, "Common controls initialized", "Debug", MB_OK);

    // Create main settings dialog
    INT_PTR result = DialogBox(hInstance, MAKEINTRESOURCE(IDD_SETTINGS_MAIN), nullptr, SettingsDialogProc);
    
    if (result == -1) {
        char errorMsg[256];
        DWORD error = GetLastError();
        sprintf_s(errorMsg, "DialogBox failed with error: %lu", error);
        MessageBox(nullptr, errorMsg, "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    MessageBox(nullptr, "Dialog completed normally", "Debug", MB_OK);
    return 0;
}

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hListView = nullptr;
    
    switch (message)
    {
    case WM_INITDIALOG:
        {
            // Set dialog title
            SetWindowText(hDlg, "SpoutCam Settings");
            
            // Center dialog on screen
            RECT rect;
            GetWindowRect(hDlg, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
            SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
            
            // Initialize list view
            hListView = GetDlgItem(hDlg, IDC_CAMERA_LIST);
            if (hListView) {
                // Set up list view columns
                LVCOLUMN lvc = {0};
                lvc.mask = LVCF_TEXT | LVCF_WIDTH;
                
                lvc.pszText = "Camera";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 0, &lvc);
                
                lvc.pszText = "Status";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 1, &lvc);
                
                lvc.pszText = "Settings";
                lvc.cx = 100;
                ListView_InsertColumn(hListView, 2, &lvc);
                
                // Set full row select
                ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
                
                PopulateCameraList(hListView);
            }
            
            return TRUE;
        }
        
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDC_CONFIGURE_CAMERA:
                {
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    if (selected != -1) {
                        OpenCameraProperties(selected);
                    } else {
                        MessageBox(hDlg, "Please select a camera to configure.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                }
                break;
                
            case IDC_REGISTER_CAMERA:
                {
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    if (selected != -1) {
                        RegisterCamera(selected);
                        RefreshCameraList(hListView);
                    } else {
                        MessageBox(hDlg, "Please select a camera to register.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                }
                break;
                
            case IDC_UNREGISTER_CAMERA:
                {
                    int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    if (selected != -1) {
                        UnregisterCamera(selected);
                        RefreshCameraList(hListView);
                    } else {
                        MessageBox(hDlg, "Please select a camera to unregister.", "SpoutCam Settings", MB_OK | MB_ICONINFORMATION);
                    }
                }
                break;
                
            case IDC_REFRESH:
                RefreshCameraList(hListView);
                break;
                
            case IDCANCEL:
            case IDOK:
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
        }
        break;
        
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == IDC_CAMERA_LIST && pnmh->code == NM_DBLCLK) {
                // Double-click to configure
                int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                if (selected != -1) {
                    OpenCameraProperties(selected);
                }
            }
        }
        break;
    }
    return FALSE;
}

void PopulateCameraList(HWND hListView)
{
    ListView_DeleteAllItems(hListView);
    
    for (int i = 0; i < MAX_CAMERAS; i++) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        
        // Camera name
        char cameraName[32];
        sprintf_s(cameraName, "SpoutCam%d", i + 1);
        lvi.pszText = cameraName;
        
        int itemIndex = ListView_InsertItem(hListView, &lvi);
        
        // Status column
        bool registered = IsCameraRegistered(i);
        ListView_SetItemText(hListView, itemIndex, 1, registered ? "Registered" : "Not Registered");
        
        // Settings column  
        bool hasSettings = HasCameraSettings(i);
        ListView_SetItemText(hListView, itemIndex, 2, hasSettings ? "Configured" : "Default");
    }
}

void RefreshCameraList(HWND hListView)
{
    PopulateCameraList(hListView);
}

bool IsCameraRegistered(int cameraIndex)
{
    // TODO: Check DirectShow filter registration
    // For now, assume all cameras can be registered
    return false;
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
    
    // Check if any settings exist
    return ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "fps", &testValue) ||
           ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "resolution", &testValue) ||
           ReadDwordFromRegistry(HKEY_CURRENT_USER, keyName, "mirror", &testValue);
}

void OpenCameraProperties(int cameraIndex)
{
    // Get the directory where this executable is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    // Detect architecture and build path to properties
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitWindows = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    
    const wchar_t* subDir = is64BitWindows ? L"SpoutCam64" : L"SpoutCam32";
    
    wchar_t cmdPath[MAX_PATH];
    swprintf_s(cmdPath, MAX_PATH, L"%s\\%s\\SpoutCamProperties.cmd", exePath, subDir);
    
    // TODO: Pass camera index as parameter to properties dialog
    // For now, just open the standard properties
    ShellExecuteW(nullptr, L"runas", cmdPath, nullptr, nullptr, SW_SHOW);
}

void RegisterCamera(int cameraIndex)
{
    // TODO: Implement camera registration logic
    char message[256];
    sprintf_s(message, "Register SpoutCam%d\n\nThis feature will be implemented to register the specific camera instance.", cameraIndex + 1);
    MessageBox(nullptr, message, "Register Camera", MB_OK | MB_ICONINFORMATION);
}

void UnregisterCamera(int cameraIndex)
{
    // TODO: Implement camera unregistration logic  
    char message[256];
    sprintf_s(message, "Unregister SpoutCam%d\n\nThis feature will be implemented to unregister the specific camera instance.", cameraIndex + 1);
    MessageBox(nullptr, message, "Unregister Camera", MB_OK | MB_ICONINFORMATION);
}

// Alternative main function for console builds
int main()
{
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}