#include <dshow.h>

// Forward declaration for single camera registration function
HRESULT RegisterSingleCameraFilter(BOOL bRegister, int cameraIndex);

class CSpoutCamProperties : public CBasePropertyPage
{

public:

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

private:

	INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HRESULT OnConnect(IUnknown *pUnknown);
	HRESULT OnDisconnect();
	HRESULT OnActivate();
	HRESULT OnDeactivate();
	HRESULT OnApplyChanges();
	STDMETHODIMP GetPageInfo(LPPROPPAGEINFO pPageInfo);

	CSpoutCamProperties(LPUNKNOWN lpunk, HRESULT *phr);

	BOOL m_bIsInitialized;          // Used to ignore startup messages
	BOOL m_bSilent;                 // Disable warnings mode
	ICamSettings *m_pCamSettings;   // The custom interface on the filter
	int m_cameraIndex;              // Index of the camera instance this dialog is configuring
	
	// Helper methods for enhanced UI
	void InitializeCameraName();
	void RefreshSenderList();
	void PopulateAvailableSenders();
	HRESULT GetCameraIndex(int* pCameraIndex);
	void SetRegistryPath(char* registryPath, int cameraIndex);
	void UpdateCameraName(const char* newName);
	void LoadCameraName();
	
	// Camera registration methods
	void RegisterCameras();
	void UnregisterCameras();
	void RegisterSingleCamera(int cameraIndex);
	void UnregisterSingleCamera(int cameraIndex);
	
	// Tab control methods for multi-camera management
	void InitializeTabControl();
	void AddCameraTab();
	void RemoveCameraTab();
	void OnTabSelectionChange();
	void UpdateCameraDisplay();
	void SaveCurrentCameraSettings();
	void LoadCameraSettings(int cameraIndex);
	void ApplyRealTimeSettings();
	void RefreshControlsDisplay();
	void AutoReregisterCamera();
	void GetActiveSenderDefaults(DWORD* pDefaultFps, DWORD* pDefaultResolution);
};

