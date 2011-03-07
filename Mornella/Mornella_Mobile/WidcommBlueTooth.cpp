#include "WidcommBlueTooth.h"
#include <regext.h>

WidcommBlueTooth::WidcommBlueTooth(GUID bt) : m_bIsBtAlreadyActive(FALSE), m_hBT(NULL),
m_hMutex(NULL), m_OEMSetBthPowerState(NULL), m_CBtIfConstructor(NULL), m_CBtIfDestructor(NULL),
m_IsStackServerUp(NULL), m_AllowToConnect(NULL), m_SetDiscoverable(NULL) {
	HRESULT hRes;
	WCHAR wDllName[MAX_PATH];

	ZeroMemory(m_wInstanceName, sizeof(m_wInstanceName));

	deviceObj = Device::self();
	statusObj = Status::self();
	m_btGuid = bt;

	// Cerchiamo la DLL che gestisce lo stack
	hRes = RegistryGetString(HKEY_LOCAL_MACHINE, 
			L"\\System\\CurrentControlSet\\Control\\Power\\BluetoothRadioOffOverride",
			L"DllName", wDllName, sizeof(wDllName));

	if (hRes == S_OK)
		m_hBT = LoadLibrary(wDllName);

	// Se non la troviamo, proviamo con le dll "standard"
	if (m_hBT == NULL)
		m_hBT = LoadLibrary(L"wbtapiCE.dll");

	if (m_hBT == NULL)
		m_hBT = LoadLibrary(L"BtFlightModeCtl.dll");

	// Non ce l'abbiamo fatta
	if (m_hBT == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - Constructor() FAILED [0]\n", 5, FALSE);
		return;
	}

	// Valorizziamo i puntatori alle funzioni che risiedono nella DLL appena caricata
	m_OEMSetBthPowerState = (t_OEMSetBthPowerState *)GetProcAddress(m_hBT, L"OEMSetBthPowerState");

	// Carichiamo anche la dll dello stack Widcomm
	m_hWidcomm = LoadLibrary(L"BtSdkCE50.dll");	

	if (m_hWidcomm == NULL)
		m_hWidcomm = LoadLibrary(L"BtSdkCE30.dll");

	// Non ce l'abbiamo fatta
	if (m_hWidcomm == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - Constructor() FAILED [1]\n", 5, FALSE);
		return;
	}

	// Valorizziamo i puntatori alle funzioni che fanno parte dello stack Widcomm
	m_CBtIfConstructor = (t_CBtIfConstructor *)GetProcAddress(m_hWidcomm, L"??0CBtIf@@QAA@XZ");
	m_CBtIfDestructor = (t_CBtIfDestructor *)GetProcAddress(m_hWidcomm, L"??1CBtIf@@UAA@XZ");
	m_IsStackServerUp = (t_IsStackServerUp *)GetProcAddress(m_hWidcomm, L"?IsStackServerUp@CBtIf@@QAAHXZ");
	m_AllowToConnect = (t_AllowToConnect *)GetProcAddress(m_hWidcomm, L"?AllowToConnect@CBtIf@@QAAXW4CONNECT_ALLOW_TYPE@1@@Z");
	m_SetDiscoverable = (t_SetDiscoverable *)GetProcAddress(m_hWidcomm, L"?SetDiscoverable@CBtIf@@QAAXH@Z");
}

WidcommBlueTooth::~WidcommBlueTooth() {
	if (m_hBT)
		FreeLibrary(m_hBT);

	if (m_hWidcomm)
		FreeLibrary(m_hWidcomm);
}

BOOL WidcommBlueTooth::SetInstanceName(PWCHAR pwName) {
	if (pwName == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - SetInstanceName() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	_snwprintf(m_wInstanceName, 33, pwName);
	DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - SetInstanceName() Performed\n", 6, FALSE);
	return TRUE;
}

void WidcommBlueTooth::SetGuid(GUID guid) {
	m_btGuid = guid;
}

BOOL WidcommBlueTooth::InquiryService(WCHAR *address) {
	if (m_hBT == NULL || m_hWidcomm == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - InquiryService() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	return FALSE;
}

BT_ADDR WidcommBlueTooth::FindServer(SOCKET bt) {
	DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - FindServer() FAILED [0] WIDCOMM UNSUPPORTED\n", 5, FALSE);
	return FALSE;

	if (m_hBT == NULL || m_hWidcomm == NULL || m_CBtIfConstructor == NULL || 
		m_CBtIfDestructor == NULL || m_IsStackServerUp == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - FindServer() FAILED [0]\n", 5, FALSE);
		return 0;
	}

	// Costruiamo l'oggetto
	PVOID CBtIfThis = m_CBtIfConstructor();

	if (m_IsStackServerUp(CBtIfThis) == FALSE) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - FindServer() FAILED [1]\n", 5, FALSE);

		// Distruttore
		m_CBtIfDestructor(CBtIfThis);
		return FALSE;
	}

	/*m_hMutex = CreateMutex(NULL, TRUE, NULL);

	if (m_hMutex == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - FindServer() FAILED [2]\n", 5, FALSE);
		return 0;
	}

	// Ogni device verra' notificato alla OnDeviceResponded()
	if (widcom.StartInquiry() == FALSE) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - FindServer() FAILED [3]\n", 5, FALSE);
		return 0;
	}

	WAIT_AND_SIGNAL(m_hMutex);*/

	// Distruttore
	m_CBtIfDestructor(CBtIfThis);
	return 0;
}

BOOL WidcommBlueTooth::ActivateBT() {
#pragma message(__LOC__"***WARNING*** STACK WIDCOMM NON IMPLEMENTATO!!!")
	DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - ActivateBT() FAILED [0] WIDCOMM UNSUPPORTED\n", 5, FALSE);
	return FALSE;

	if (m_hBT == NULL || m_hWidcomm == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - ActivateBT() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	// Scopriamo se il BT e' gia' attivo
	if (IsBTActive()) {
		m_bIsBtAlreadyActive = TRUE;
		return TRUE;
	} else {
		m_bIsBtAlreadyActive = FALSE;
	}

	// Costruiamo l'oggetto
	PVOID CBtIfThis = m_CBtIfConstructor();

	m_AllowToConnect(CBtIfThis, CONNECT_ALLOW_ALL);
	m_SetDiscoverable(CBtIfThis, FALSE);

	m_CBtIfDestructor(CBtIfThis);
	return TRUE;
}

BOOL WidcommBlueTooth::DeActivateBT() {
	if (m_hBT == NULL || m_hWidcomm == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - DeActivateBT() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	if (m_bIsBtAlreadyActive)
		return TRUE;

	if (m_OEMSetBthPowerState == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - DeActivateBT() FAILED [1]\n", 5, FALSE);
		return FALSE;
	}

	m_OEMSetBthPowerState(FALSE);
	return TRUE;
}

BOOL WidcommBlueTooth::IsBTActive() {
	HWND btHwnd;
	UINT uStackStatus;
	DWORD dwRes;

	btHwnd = FindWindow(L"WCE_BTTRAY", 0);

	if (m_hBT == NULL || m_hWidcomm == NULL) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - IsBTActive() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	uStackStatus = RegisterWindowMessage(L"WIDCOMM_WM_GETSTACKSTATUS");

	if (uStackStatus == 0) {
		DBG_TRACE(L"Debug - WidcommBlueTooth.cpp - IsBTActive() FAILED [1]\n", 5, FALSE);
		return FALSE;
	}

	dwRes =	SendMessage(btHwnd, uStackStatus, 0, 0);

	if (dwRes > 0)
		return TRUE;

	return FALSE;
}

void WidcommBlueTooth::OnDeviceResponded(BD_ADDR p_bda, DEV_CLASS dev_class, BD_NAME bd_name, BOOL bConnected) {
	/*CDeviceFoundPackage *pDeviceFoundPackage = new CDeviceFoundPackage;
	
	CopyMemory(pDeviceFoundPackage->m_p_bda, p_bda, BD_ADDR_LEN);
	CopyMemory(pDeviceFoundPackage->m_dev_class, dev_class, DEV_CLASS_LEN);
	CopyMemory(pDeviceFoundPackage->m_bd_name, bd_name,	BD_NAME_LEN);
	
	pDeviceFoundPackage->m_bConnected = bConnected;*/
}