/**
 *	DllMain
 **/
#define INITGUID
#include <windows.h>
#include <cemapi.h>
#include "factory.h"
#include "mapirule.h"

int g_cServerLocks = 0;		// Lock Server ... 

#define DLL_FOLDER TEXT("\\Windows")

DEFINE_GUID(CLSID_TMapiRule, 0xdd69a982, 0x6c70, 0x4a55, 0x8b, 0xe4, 0x6b, 0x32, 0xa1, 0xf9, 0xa5, 0x27);

#define _TEXT_CLSIDKEY TEXT("\\CLSID\\{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}")
#define _TEXT_CLSID TEXT("{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}")

#define DLL_NAME TEXT("SmsFilter.dll")

//extern "C" __declspec(dllexport) HRESULT DllRegisterServer();
BOOL WINAPI _DllMain(HANDLE hInst, DWORD dwReason, LPVOID lpv)
{
	switch(dwReason) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_PROCESS_DETACH:
			break;

		default:
			break;
	}

	return TRUE;
}

/**
 *	DllRegisterServer
 **/
STDAPI DllRegisterServer()
{
	DWORD lastError = 0;
    LRESULT lr;
    HRESULT hr = E_FAIL;
    HKEY hKey = NULL;
    HKEY hSubKey = NULL;
    DWORD dwDisposition;
    TCHAR wszValue[127];
	TCHAR wszPath[127];

    // Set up registry keys
    // Register with COM:
    //    [HKEY_CLASSES_ROOT\CLSID\{3AB4C10E-673C-494c-98A2-CC2E91A48115}\InProcServer32]
    //    @="mapirule.dll"
	
	DBG_TRACE(L"Creating/opening key:", DBG_NOTIFY, FALSE);
	DBG_TRACE(_TEXT_CLSIDKEY, DBG_NOTIFY, FALSE);
    lr = RegCreateKeyEx(HKEY_CLASSES_ROOT, _TEXT_CLSIDKEY, 0, NULL, 0, 0, NULL,  &hKey, &dwDisposition);
	if (lr != ERROR_SUCCESS)
    {
		DBG_TRACE(L"ERROR creating/opening key.", DBG_NOTIFY, FALSE);
        goto Exit;
    }
	
	DBG_TRACE(L"Creating/opening key:", DBG_NOTIFY, FALSE);
	DBG_TRACE(TEXT("InprocServer32"), DBG_NOTIFY, FALSE);
    lr = RegCreateKeyEx(hKey, TEXT("InprocServer32"), 0, NULL, 0, 0, NULL,  &hSubKey, &dwDisposition);
	if (lr != ERROR_SUCCESS)
    {
		DBG_TRACE(L"ERROR creating/opening key.", DBG_NOTIFY, FALSE);
        goto Exit;
    }

	wcscpy(wszPath, DLL_FOLDER);
	wsprintf(wszValue, TEXT("%s\\%s"), wszPath, DLL_NAME);
	
	DBG_TRACE(L"Setting value of InprocServer32 to:", DBG_NOTIFY, FALSE);
	DBG_TRACE(wszValue, DBG_NOTIFY, FALSE);
    lr = RegSetValueEx(hSubKey, NULL, 0, REG_SZ, (LPBYTE) wszValue, (lstrlen(wszValue) + 1) * sizeof(TCHAR));
	if (lr != ERROR_SUCCESS) 
	{
		DBG_TRACE(L"ERROR setting value of InprocServer32.", DBG_NOTIFY, FALSE);
		goto Exit;
	}

	RegCloseKey(hSubKey);
	hSubKey = NULL;
	RegCloseKey(hKey);
	hKey = NULL;

    // Register with Inbox:
    //    [HKEY_LOCAL_MACHINE\Software\Microsoft\Inbox\Svc\SMS\Rules]
    //    {3AB4C10E-673C-494c-98A2-CC2E91A48115}"=dword:1

	DBG_TRACE(L"Creating/opening key:", DBG_NOTIFY, FALSE);
	DBG_TRACE(TEXT("\\Software\\Microsoft\\Inbox\\Svc\\SMS\\Rules"), DBG_NOTIFY, FALSE);
    lr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("\\Software\\Microsoft\\Inbox\\Svc\\SMS\\Rules"),
	                              0, NULL, 0, 0, NULL, 
	                              &hKey, &dwDisposition);
    if (lr != ERROR_SUCCESS)
    {
		DBG_TRACE(L"ERROR creating/opening key.", DBG_NOTIFY, FALSE);
		goto Exit;
    }
	
	dwDisposition = 1;
	DBG_TRACE(L"Setting value of rule {DD69A982-6C70-4a55-8BE4-6B32A1F9A527} to 1.", DBG_NOTIFY, FALSE);
	lr = RegSetValueEx(hKey, _TEXT_CLSID, 0, REG_DWORD, (LPBYTE) &dwDisposition, sizeof(DWORD));
    if (lr != ERROR_SUCCESS)
    {
		DBG_TRACE(L"ERROR setting value of {DD69A982-6C70-4a55-8BE4-6B32A1F9A527}.", DBG_NOTIFY, FALSE);
        goto Exit;
    }
 
    hr = S_OK;

Exit:
    if (hSubKey)
    {
        RegCloseKey(hSubKey);
    }

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return hr;
}

/**
 *	DllUnregisterServer
 **/
STDAPI DllUnregisterServer()
{
	HKEY hKey = NULL;
    HRESULT hr = E_FAIL;
    LRESULT lr;
    DWORD dwDisposition;

    // Delete registry keys
    RegDeleteKey(HKEY_CLASSES_ROOT, _TEXT_CLSIDKEY);
    
    lr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("\\Software\\Microsoft\\Inbox\\Svc\\SMS\\Rules"),
	                              0, NULL, 0, 0, NULL, 
	                              &hKey, &dwDisposition);
    if (lr != ERROR_SUCCESS)
    {
        goto Exit;
    }

    RegDeleteValue(hKey, _TEXT_CLSID);

    hr = S_OK;

Exit:
    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return hr;
}

STDAPI DllGetClassObject(const CLSID &clsid, REFIID iid, LPVOID *ppv)
{
	if (clsid != CLSID_TMapiRule) {
		return CLASS_E_CLASSNOTAVAILABLE;
	}

	CFactory *pFactory = new CFactory();

	if (!pFactory)
		return E_OUTOFMEMORY;

	HRESULT hr = pFactory->QueryInterface(iid, ppv);

	pFactory->Release();

	return hr;
}

STDAPI DllCanUnloadNow()
{
	return (!g_cServerLocks) ? S_OK : S_FALSE;
}

/**
 *	SetService
 **/
STDAPI SetService(LPWSTR ServiceProvider)
{
	HKEY hKey = NULL;
	HRESULT hr = E_FAIL;

	hr = RegOpenKeyEx(HKEY_CLASSES_ROOT, _TEXT_CLSIDKEY, 0, 0, &hKey);

	if (hr == ERROR_SUCCESS) 
	{
		RegSetValueExW(hKey, TEXT("Service Provider"), 0, REG_SZ, (LPBYTE)ServiceProvider, wcslen(ServiceProvider) * sizeof(WCHAR));
		RegCloseKey(hKey);
		RegFlushKey(HKEY_CLASSES_ROOT);
	}

	return hr;
}
