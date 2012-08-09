#include "GPRSConnection.h"
#include "Common.h"

#include <windows.h>
#include <commctrl.h>
#include <initguid.h>
#include <connmgr.h>
#include <connmgr_status.h>
#include <connmgr_conntypes.h>
#include <ras.h>
#include <winsock2.h>
#include <regext.h>
#include <winioctl.h>

// RIL IO-control codes 
#define FILE_DEVICE_RIL                             0x300 
#define IOCTL_RIL_SETGPRSATTACHED                   CTL_CODE(FILE_DEVICE_RIL, 125, METHOD_BUFFERED, FILE_ANY_ACCESS)

GPRSConnection::GPRSConnection()
: _hConnection(INVALID_HANDLE_VALUE), _hConnMgr(INVALID_HANDLE_VALUE), dwCacheVal(60), bForced(FALSE)
{
	ZeroMemory(wString, sizeof(wString));

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\ConnMgr\\Planner\\Settings", 0, 0, &hKey) != ERROR_SUCCESS)
		return;

	// Preleviamo i valori originali
	RegistryGetDWORD(hKey, NULL, L"CacheTime", &dwCacheVal);
	RegistryGetString(hKey, NULL, L"SuspendResume", wString, MAX_PATH);

	// Riduciamo il cache time della connessione GPRS
	RegistrySetDWORD(hKey, NULL, L"CacheTime", 10);

	// Diciamo al telefono di troncare la connessione allo standby
	//RegistrySetString(hKey, NULL, L"SuspendResume", L"GPRS_bye_if_device_off");
	RegistrySetString(hKey, NULL, L"SuspendResume", L"~GPRS!");

	RegCloseKey(hKey);
}

GPRSConnection::~GPRSConnection()
{
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\ConnMgr\\Planner\\Settings", 0, 0, &hKey) != ERROR_SUCCESS)
		return;

	// Ripristiniamo i parametri della connessione GPRS
	RegistrySetDWORD(hKey, NULL, L"CacheTime", dwCacheVal);

	// GPRS_bye_if_device_off e ~GPRS!
	RegistrySetString(hKey, NULL, L"SuspendResume", wString);

	RegCloseKey(hKey);
}

bool GPRSConnection::_connect()
{
	CONNMGR_CONNECTIONINFO ConnectionInfo;

	if (isGprsConnected()) {
		return true;
	}

	ZeroMemory (&ConnectionInfo, sizeof (ConnectionInfo));
	ConnectionInfo.cbSize = sizeof (ConnectionInfo);

	ConnectionInfo.dwParams = CONNMGR_PARAM_GUIDDESTNET; 
	ConnectionInfo.dwFlags = CONNMGR_FLAG_NO_ERROR_MSGS; 
	ConnectionInfo.dwPriority = CONNMGR_PRIORITY_HIPRIBKGND; 
	ConnectionInfo.guidDestNet = IID_DestNetInternet;

	DWORD dwStatus;
	HRESULT hr = ConnMgrEstablishConnectionSync (&ConnectionInfo, &_hConnection, 30000, &dwStatus);

	if (FAILED(hr)) {
		DBG_TRACE_INT(L"Debug - GPRSConnection.cpp - _connect [ConnMgrEstablishConnectionSync()] error: ", 5, FALSE, hr);
		return false;
	}

	bForced = TRUE;
	DBG_TRACE(L"Debug - GPRSConnection.cpp - _connect [ConnMgrEstablishConnectionSync()] OK\n", 6, FALSE);
	return true;
}

bool GPRSConnection::_disconnect()
{
	HANDLE hDev = INVALID_HANDLE_VALUE;
	UINT uVal = 0, uRet;
	DWORD dwBytesRet;

	if (bForced == FALSE) {
		return true;
	}

	HRESULT hr = ConnMgrReleaseConnection(_hConnection, FALSE);

	if (FAILED(hr)) {
		DBG_TRACE_INT(L"Debug - GPRSConnection.cpp - _disconnect [ConnMgrReleaseConnection()] error: ", 5, FALSE, hr);
		return false;
	}

	bForced = FALSE;
	DBG_TRACE(L"Debug - GPRSConnection.cpp - _disconnect [ConnMgrReleaseConnection()] OK\n", 6, FALSE);

	// Proviamo a chiudere la connessione GPRS con le RIL
	hDev = CreateFile(L"RIL1:", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);

	if (hDev == INVALID_HANDLE_VALUE) {
		DBG_TRACE(L"Debug - GPRSConnection.cpp - _disconnect() [FAILED to open RIL1:]\n", 6, FALSE);
		return false;
	}

	// Equivale a una RIL_SetGPRSAttached(..., FALSE);
	if (DeviceIoControl(hDev, IOCTL_RIL_SETGPRSATTACHED, (PVOID)&uVal, sizeof(uVal), &uRet, sizeof(uRet), &dwBytesRet, NULL) == FALSE) {
		DBG_TRACE(L"Debug - GPRSConnection.cpp - _disconnect() [FAILED DeviceIoControl() on RIL1:]\n", 6, FALSE);
	}

	CloseHandle(hDev);
	return true;
}

bool GPRSConnection::isGprsConnected() {
	CONNMGR_CONNECTION_DETAILED_STATUS *pStatus = NULL, *ptr;
	HRESULT hr;
	DWORD dwSize = 0;

	hr = ConnMgrQueryDetailedStatus(pStatus, &dwSize);

	if (hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
		pStatus = (CONNMGR_CONNECTION_DETAILED_STATUS *)new(std::nothrow) BYTE[dwSize + 0x100];
		dwSize += 0x100;
	}

	if (pStatus == NULL) {
		return false;
	}

	hr = ConnMgrQueryDetailedStatus(pStatus, &dwSize);

	if (FAILED(hr)) {
		delete[] pStatus;
		return false;
	}

	// Controlliamo che ci sia almeno una connessione di tipo cellular, connessa e con un IP
	for (ptr = pStatus; ptr != NULL; ptr = ptr->pNext) {
		if (ptr->dwType == CM_CONNTYPE_CELLULAR && ptr->dwConnectionStatus == CONNMGR_STATUS_CONNECTED &&
			ptr->pIPAddr) {
			delete[] pStatus;
			return true;
		}
	}

	delete[] pStatus;
	return false;
}