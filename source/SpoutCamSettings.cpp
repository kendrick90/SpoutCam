#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include "../SpoutDX/source/SpoutDX.h"

// Camera CLSID definitions
const GUID SPOUTCAM_CLSID_BASE = {0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33};

// Resource IDs
#define IDD_SETTINGS_MAIN       100
#define IDC_REGISTER_NEW        101
#define IDC_CAMERA_TABS         102
#define IDC_CAMERA_LIST         103
#define IDC_UNREGISTER_SELECTED 104

struct CameraInfo {
    int index;
    std::wstring name;
    GUID clsid;
    bool registered;
};

class SpoutCamSettings {
private:
    HINSTANCE m_hInstance;
    HWND m_hDialog;
    std::vector<CameraInfo> m_cameras;

public:
    SpoutCamSettings(HINSTANCE hInstance) : m_hInstance(hInstance), m_hDialog(nullptr) {
        InitializeCameraList();
    }

    void InitializeCameraList() {
        // Initialize potential cameras (up to 8)
        for (int i = 0; i < 8; i++) {
            CameraInfo camera;
            camera.index = i;
            camera.name = (i == 0) ? L"SpoutCam" : L"SpoutCam" + std::to_wstring(i + 1);
            
            // Generate CLSID by incrementing last byte
            camera.clsid = SPOUTCAM_CLSID_BASE;
            camera.clsid.Data4[7] = (BYTE)(0x33 + i);
            camera.registered = IsRegistered(camera.clsid);
            
            m_cameras.push_back(camera);
        }
    }

    bool IsRegistered(const GUID& clsid) {
        // Check if camera is registered by trying to create it
        HKEY hKey;
        wchar_t keyPath[256];
        swprintf_s(keyPath, L"CLSID\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                   clsid.Data1, clsid.Data2, clsid.Data3,
                   clsid.Data4[0], clsid.Data4[1], clsid.Data4[2], clsid.Data4[3],
                   clsid.Data4[4], clsid.Data4[5], clsid.Data4[6], clsid.Data4[7]);
        
        LONG result = RegOpenKeyEx(HKEY_CLASSES_ROOT, keyPath, 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

    void RegisterNewCamera() {
        // Find first unregistered camera
        for (auto& camera : m_cameras) {
            if (!camera.registered) {
                HRESULT hr = RegisterSingleCamera(camera.index);
                if (SUCCEEDED(hr)) {
                    camera.registered = true;
                    MessageBox(m_hDialog, L"Camera registered successfully!", L"Registration", MB_OK | MB_ICONINFORMATION);
                    RefreshCameraList();
                } else {
                    MessageBox(m_hDialog, L"Failed to register camera.", L"Registration Error", MB_OK | MB_ICONERROR);
                }
                return;
            }
        }
        MessageBox(m_hDialog, L"Maximum number of cameras already registered.", L"Registration", MB_OK | MB_ICONWARNING);
    }

    void UnregisterSelectedCamera() {
        HWND hList = GetDlgItem(m_hDialog, IDC_CAMERA_LIST);
        int selected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
        if (selected >= 0 && selected < (int)m_cameras.size()) {
            auto& camera = m_cameras[selected];
            if (camera.registered) {
                HRESULT hr = UnregisterSingleCamera(camera.index);
                if (SUCCEEDED(hr)) {
                    camera.registered = false;
                    MessageBox(m_hDialog, L"Camera unregistered successfully!", L"Unregistration", MB_OK | MB_ICONINFORMATION);
                    RefreshCameraList();
                } else {
                    MessageBox(m_hDialog, L"Failed to unregister camera.", L"Unregistration Error", MB_OK | MB_ICONERROR);
                }
            }
        } else {
            MessageBox(m_hDialog, L"Please select a camera to unregister.", L"Unregistration", MB_OK | MB_ICONWARNING);
        }
    }

    HRESULT RegisterSingleCamera(int index) {
        // Use regsvr32 to register the SpoutCam DLL
        wchar_t dllPath[MAX_PATH];
        GetModuleFileName(nullptr, dllPath, MAX_PATH);
        
        // Replace .exe with .ax (assuming SpoutCamSettings.exe is in same folder as SpoutCam64.ax)
        std::wstring path(dllPath);
        size_t pos = path.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            path = path.substr(0, pos + 1) + L"SpoutCam64.ax";
        }
        
        // Use ShellExecute to run regsvr32 with admin privileges
        SHELLEXECUTEINFO sei = {};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = L"regsvr32.exe";
        sei.lpParameters = (L"\"" + path + L"\"").c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        if (ShellExecuteEx(&sei)) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD exitCode;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
            return (exitCode == 0) ? S_OK : E_FAIL;
        }
        return E_FAIL;
    }

    HRESULT UnregisterSingleCamera(int index) {
        // Use regsvr32 to unregister the SpoutCam DLL
        wchar_t dllPath[MAX_PATH];
        GetModuleFileName(nullptr, dllPath, MAX_PATH);
        
        // Replace .exe with .ax
        std::wstring path(dllPath);
        size_t pos = path.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            path = path.substr(0, pos + 1) + L"SpoutCam64.ax";
        }
        
        // Use ShellExecute to run regsvr32 /u with admin privileges
        SHELLEXECUTEINFO sei = {};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = L"regsvr32.exe";
        sei.lpParameters = (L"/u \"" + path + L"\"").c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        if (ShellExecuteEx(&sei)) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD exitCode;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
            return (exitCode == 0) ? S_OK : E_FAIL;
        }
        return E_FAIL;
    }

    void RefreshCameraList() {
        // Refresh registration status
        for (auto& camera : m_cameras) {
            camera.registered = IsRegistered(camera.clsid);
        }
        UpdateCameraListView();
    }

    void UpdateCameraListView() {
        HWND hList = GetDlgItem(m_hDialog, IDC_CAMERA_LIST);
        ListView_DeleteAllItems(hList);

        for (size_t i = 0; i < m_cameras.size(); i++) {
            const auto& camera = m_cameras[i];
            
            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem = (int)i;
            lvi.lParam = (LPARAM)i;
            lvi.pszText = (LPWSTR)camera.name.c_str();
            ListView_InsertItem(hList, &lvi);
            
            // Status column
            ListView_SetItemText(hList, (int)i, 1, camera.registered ? L"Registered" : L"Not Registered");
        }
    }

    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
        static SpoutCamSettings* pThis = nullptr;
        
        switch (message) {
        case WM_INITDIALOG:
            pThis = reinterpret_cast<SpoutCamSettings*>(lParam);
            pThis->m_hDialog = hDlg;
            pThis->InitializeDialog();
            return TRUE;

        case WM_COMMAND:
            if (pThis) {
                switch (LOWORD(wParam)) {
                case IDC_REGISTER_NEW:
                    pThis->RegisterNewCamera();
                    break;
                case IDC_UNREGISTER_SELECTED:
                    pThis->UnregisterSelectedCamera();
                    break;
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
                }
            }
            break;
        }
        return FALSE;
    }

    void InitializeDialog() {
        // Setup list view
        HWND hList = GetDlgItem(m_hDialog, IDC_CAMERA_LIST);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        
        // Add columns
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.cx = 120;
        lvc.pszText = L"Camera Name";
        ListView_InsertColumn(hList, 0, &lvc);
        
        lvc.cx = 100;
        lvc.pszText = L"Status";
        ListView_InsertColumn(hList, 1, &lvc);
        
        UpdateCameraListView();
    }

    int Run() {
        return (int)DialogBoxParam(m_hInstance, MAKEINTRESOURCE(IDD_SETTINGS_MAIN), 
                                   nullptr, DialogProc, reinterpret_cast<LPARAM>(this));
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow) {
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    SpoutCamSettings app(hInstance);
    return app.Run();
}