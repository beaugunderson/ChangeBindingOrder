#include "stdafx.h"

#define BUFFER_SIZE 4096

WCHAR wDeviceInstanceId[MAX_PATH]; 

inline ULONG ReleaseObj(IUnknown* punk)
{
	return punk ? punk->Release() : 0;
}

void FillDeviceInstanceId(HDEVINFO hDevHandle, SP_DEVINFO_DATA deviceInfoData, WCHAR *wAdapter)
{
	DWORD i;

	for (i = 0; SetupDiEnumDeviceInfo(hDevHandle, i, &deviceInfoData); i++)
	{
		BYTE buffer[BUFFER_SIZE];
		WCHAR deviceInfo[BUFFER_SIZE];
		DWORD dwProperty = 0; 

		memset(buffer, 0, BUFFER_SIZE);

		if (SetupDiGetDeviceRegistryProperty(hDevHandle, &deviceInfoData,
			SPDRP_DEVICEDESC , &dwProperty, buffer, BUFFER_SIZE, NULL) == TRUE)
		{
			wcscpy_s(deviceInfo, BUFFER_SIZE, (LPWSTR)buffer);

			if (wcscmp(wAdapter, deviceInfo) == 0)
			{
				RtlZeroMemory(wDeviceInstanceId, MAX_PATH);

				SetupDiGetDeviceInstanceId(hDevHandle, &deviceInfoData, wDeviceInstanceId, MAX_PATH, 0);
			}
		}
	}
} 

bool ReleaseINetCfg(BOOL bHasWriteLock, INetCfg* pNetCfg)
{
	// If write lock is present, unlock it
	if (SUCCEEDED(pNetCfg->Uninitialize()) && bHasWriteLock)
	{
		INetCfgLock* pNetCfgLock;

		// Get the locking interface
		if (SUCCEEDED(pNetCfg->QueryInterface(IID_INetCfgLock, (LPVOID *)&pNetCfgLock)))
		{
			pNetCfgLock->ReleaseWriteLock();

			ReleaseObj(pNetCfgLock);
		}
	}

	ReleaseObj(pNetCfg);

	CoUninitialize(); 

	return true;
}

bool ChangeNicBindingOrder()
{
	HRESULT hr = S_OK;

	INetCfg *pNetCfg = NULL; 
	INetCfgBindingPath *pNetCfgPath; 
	INetCfgComponent *pNetCfgComponent = NULL;
	INetCfgComponentBindings *pNetCfgBinding = NULL;
	INetCfgLock *pNetCfgLock = NULL;

	IEnumNetCfgBindingPath *pEnumNetCfgBindingPath = NULL;

	PWSTR szLockedBy;

	if (!SUCCEEDED(CoInitialize(NULL)))
	{
		return false;
	}

	if (S_OK != CoCreateInstance(CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER, IID_INetCfg, (void**)&pNetCfg))
	{
		return false;
	}

	if (!SUCCEEDED(pNetCfg->QueryInterface(IID_INetCfgLock, (LPVOID *)&pNetCfgLock)))
	{
		return false;
	}

	static const ULONG c_cmsTimeout = 5000;
	static const WCHAR c_szSampleNetcfgApp[] = L"TapRebinder (TapRebinder.exe)";

	if (!SUCCEEDED(pNetCfgLock->AcquireWriteLock(c_cmsTimeout, c_szSampleNetcfgApp, &szLockedBy)))
	{
		wprintf(L"Could not lock INetcfg, it is already locked by '%s'", szLockedBy);

		return false;
	}

	if (!SUCCEEDED(pNetCfg->Initialize(NULL)))
	{
		if (pNetCfgLock)
		{
			pNetCfgLock->ReleaseWriteLock();
		}

		ReleaseObj(pNetCfgLock);

		return false;
	}

	ReleaseObj(pNetCfgLock);

	if (S_OK != pNetCfg->FindComponent(L"ms_tcpip", &pNetCfgComponent))
	{
		return false;
	}

	if (S_OK != pNetCfgComponent->QueryInterface(IID_INetCfgComponentBindings, (LPVOID *)&pNetCfgBinding))
	{
		return false;
	}

	if (S_OK != pNetCfgBinding->EnumBindingPaths(EBP_BELOW, &pEnumNetCfgBindingPath))
	{
		return false;
	}

	while (S_OK == hr)
	{
		hr = pEnumNetCfgBindingPath->Next(1, &pNetCfgPath, NULL);

		LPWSTR pszwPathToken;

		pNetCfgPath->GetPathToken(&pszwPathToken);

		if (wcscmp(pszwPathToken, wDeviceInstanceId) == 0)
		{
			wprintf(L"   Moving adapter to the first position: %s.\n", pszwPathToken);

			pNetCfgBinding->MoveBefore(pNetCfgPath, NULL);
			pNetCfg->Apply();

			CoTaskMemFree(pszwPathToken);

			ReleaseObj(pNetCfgPath);

			break;
		}

		CoTaskMemFree(pszwPathToken);

		ReleaseObj(pNetCfgPath);
	}

	ReleaseObj(pEnumNetCfgBindingPath);

	ReleaseObj(pNetCfgBinding);
	ReleaseObj(pNetCfgComponent);

	ReleaseINetCfg(TRUE, pNetCfg);

	return true;
} 

int _tmain(int argc, _TCHAR* argv[])
{
	WCHAR *wDeviceName = L"TAP-Win32 Adapter V9";
	SP_DEVINFO_DATA deviceInfoData;
	HDEVINFO hDevHandle;

	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	hDevHandle = SetupDiGetClassDevs(&GUID_DEVCLASS_NET, NULL, NULL, DIGCF_PRESENT);

	wcscpy_s(wDeviceInstanceId, MAX_PATH, L"NULL");

	FillDeviceInstanceId(hDevHandle, deviceInfoData, wDeviceName);

	SetupDiDestroyDeviceInfoList(hDevHandle); 

	if (wcscmp(wDeviceInstanceId, L"NULL") == 0)
	{
		wprintf(L"An error occurred.\n");

		return 1;
	} 

	WCHAR wBind[1024];

	RtlZeroMemory(wBind, 1024);

	// Prepend 'ms_tcpip->' to the device instance ID
	wcscpy_s(wBind, L"ms_tcpip->");
	wcscat_s(wBind, wDeviceInstanceId);

	RtlZeroMemory(wDeviceInstanceId, MAX_PATH);

	wcscpy_s(wDeviceInstanceId, MAX_PATH, wBind);

	wprintf(L"Changing binding order for %s...\n", wDeviceInstanceId);

	ChangeNicBindingOrder();

	return 0;
}