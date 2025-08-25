#include <dshow.h>
#include <string>
#include "DynamicCameraManager.h"

// Forward declarations for camera activation functions
HRESULT RegisterSingleCameraFilter(BOOL bRegister, int cameraIndex);
STDAPI RegisterCameraByName(const char* cameraName);
STDAPI UnregisterCameraByName(const char* cameraName);

class CSpoutCamProperties : public CBasePropertyPage
{

public:

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
	// Camera-specific factory function that knows which camera it represents
	static CUnknown * WINAPI CreateInstanceForCamera(LPUNKNOWN lpunk, HRESULT *phr, const std::string& cameraName);

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
	std::string m_cameraName;       // Name of the camera this dialog represents
	
	// Helper methods for enhanced UI
	void InitializeCameraName();
	void RefreshSenderList();
	void PopulateAvailableSenders();
	HRESULT GetCameraIndex(int* pCameraIndex);
	void SetRegistryPath(char* registryPath, int cameraIndex);
	void GetCameraRegistryPath(char* registryPath, size_t bufferSize);
	void UpdateCameraName(const char* newName);
	void LoadCameraName();
	
	// Camera activation methods
	void ActivateCameras();
	void DeactivateCameras();
	void ActivateSingleCamera(int cameraIndex);
	void DeactivateSingleCamera(int cameraIndex);
	void ActivateCurrentCamera();
	void DeactivateCurrentCamera();
	
	// Properties initialization and legacy tab compatibility functions
	void InitializeProps();
	void AddCameraTab();
	void RemoveCameraTab();
	void OnTabSelectionChange();
	void UpdateCameraDisplay();
	void SaveCurrentCameraSettings();
	void LoadCameraSettings(int cameraIndex);
	void ApplyRealTimeSettings();
	void RefreshControlsDisplay();
	void AutoReactivateCamera();
	void GetActiveSenderDefaults(DWORD* pDefaultFps, DWORD* pDefaultResolution);
};

