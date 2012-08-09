#include "WindowsBlueTooth.h"
#include "Common.h"
#include <new>
#include <Bt_api.h>
#include <bthutil.h>
#include <bthapi.h>
#include <InitGuid.h>

#define QUERYSET_SIZE (5 * 1024)

WindowsBlueTooth::WindowsBlueTooth(GUID bt) : m_bIsBtAlreadyActive(FALSE){
	ZeroMemory(m_wInstanceName, sizeof(m_wInstanceName));

	deviceObj = Device::self();
	statusObj = Status::self();
	m_btGuid = bt;
}

WindowsBlueTooth::~WindowsBlueTooth() {

}

BOOL WindowsBlueTooth::SetInstanceName(PWCHAR pwName) {
	if (pwName == NULL)
		return FALSE;

	_snwprintf(m_wInstanceName, 33, pwName);
	return TRUE;
}

void WindowsBlueTooth::SetGuid(GUID guid) {
	m_btGuid = guid;
}

BOOL WindowsBlueTooth::InquiryService(WCHAR *address) {
	WSAQUERYSET querySet;
	GUID protocol;
	DWORD bufferLength;
	WSAQUERYSET *pResults;
	HANDLE handle;
	BTHNS_RESTRICTIONBLOB rb;
	BLOB blob;
	BOOL state;

	pResults = (WSAQUERYSET *)new(std::nothrow) BYTE[QUERYSET_SIZE];

	if (pResults == NULL) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - InquiryService() FAILED [0] ", 5, TRUE);
		return 0;
	}

	ZeroMemory(&blob, sizeof(BLOB));
	ZeroMemory(&rb, sizeof(BTHNS_RESTRICTIONBLOB));
	ZeroMemory(&querySet, sizeof(WSAQUERYSET));
	ZeroMemory(pResults, QUERYSET_SIZE);

	blob.cbSize = sizeof(BTHNS_RESTRICTIONBLOB);
	blob.pBlobData = (BYTE *)&rb;

	protocol = m_btGuid;

	querySet.dwSize = sizeof(WSAQUERYSET);
	querySet.lpServiceClassId = &protocol;
	querySet.dwNameSpace = NS_BTH;
	querySet.lpszContext = address;
	querySet.lpBlob = &blob;

	rb.type = SDP_SERVICE_SEARCH_REQUEST;
	rb.uuids[0].uuidType = SDP_ST_UUID128;
	rb.uuids[0].u.uuid128 = m_btGuid;

	// Il valore di ritorno non e' documentato quindi non effettuiamo il check
	WSALookupServiceBegin(&querySet, 0, &handle);

	bufferLength = QUERYSET_SIZE;

	if (WSALookupServiceNext(handle, LUP_RETURN_BLOB, &bufferLength, pResults)) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - InquiryService() FAILED [1] ", 5, TRUE);
#ifdef _DEBUG
		INT err = WSAGetLastError();
#endif
		state = FALSE;
	} else {
		state = TRUE;
	}

	WSALookupServiceEnd(handle);
	delete[] pResults;
	DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - InquiryService() Performed\n", 5, FALSE);
	return state;
}

BT_ADDR WindowsBlueTooth::FindServer(SOCKET bt) {
	WSAQUERYSET querySet, *pResults;
	DWORD bufferLength;
	WCHAR addressAsString[64];
	HANDLE handle;
	GUID protocol;
	INT proto = sizeof(WSAPROTOCOL_INFO);
	UINT i = 0;
	BT_ADDR btAddr = 0;

	pResults = (WSAQUERYSET *)new(std::nothrow) BYTE[QUERYSET_SIZE];

	if (pResults == NULL) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [0]\n", 5, FALSE);
		return 0;
	}

	bufferLength = QUERYSET_SIZE;

	ZeroMemory(&querySet, sizeof(WSAQUERYSET));
	ZeroMemory(pResults, QUERYSET_SIZE);

	querySet.dwSize = sizeof(WSAQUERYSET);
	querySet.dwNameSpace = NS_BTH;

	protocol = m_btGuid;

	if (WSALookupServiceBegin(&querySet, LUP_CONTAINERS, &handle)) {
		delete[] pResults;
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [1] ", 5, TRUE);
		return 0;
	}

	LOOP {
		i++;

		// Condizione di uscita addizionale nel caso di loop sullo stack BT
		if (i > 200)
			break;

		bufferLength = QUERYSET_SIZE;

		if (WSALookupServiceNext(handle, LUP_RETURN_ADDR | LUP_RETURN_NAME, &bufferLength, pResults)) {
			DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [2] ", 5, TRUE);
			break;
		}

		if (pResults->lpcsaBuffer == NULL || pResults->dwNumberOfCsAddrs != 1) {
			DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [3]\n", 5, FALSE);
			continue;
		}

		if (pResults->lpcsaBuffer->RemoteAddr.lpSockaddr == NULL) {
			DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [4]\n", 5, FALSE);
			continue;
		}

		wsprintf(addressAsString, L"(%.2X:%.2X:%.2X:%.2X:%.2X:%.2X)", (UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[11],
			(UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[10], (UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[9],
			(UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[8], (UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[7],
			(UCHAR)pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data[6]);

		// Quera solo i device BT che hanno il nome specificato nella configurazione
		if (pResults->lpszServiceInstanceName && wcslen(pResults->lpszServiceInstanceName) && !wcsicmp(m_wInstanceName, pResults->lpszServiceInstanceName)) {
			if (InquiryService(addressAsString)) {
				CopyMemory(&btAddr, pResults->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_data + 6, 6); 
				break;
			} else {
				DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [5]\n", 5, FALSE);
			}
		} else {
			DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [Not our BT device] [6]\n", 5, FALSE);
		}

		// Usciamo dal loop se l'utente riprende ad interagire col telefono
		if (deviceObj->IsDeviceUnattended() == FALSE || (statusObj->Crisis() & CRISIS_SYNC) == CRISIS_SYNC) {
			DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() FAILED [Power status or crisis changed] [7]\n", 5, FALSE);
			break;
		}
	}

	WSALookupServiceEnd(handle);
	delete[] pResults;
	DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - FindService() Performed\n", 5, FALSE);
	return btAddr;
}

BOOL WindowsBlueTooth::ActivateBT() {
	DWORD dwRadioMode = 0;
	INT iErr;

	iErr = BthGetMode(&dwRadioMode);

	if (iErr != ERROR_SUCCESS) {
		DBG_TRACE_INT(L"Debug - WindowsBlueTooth.cpp - ActivateBT() FAILED [0] ", 5, TRUE, iErr);
		return FALSE;
	}

	switch(dwRadioMode){
		case BTH_CONNECTABLE:
		case BTH_DISCOVERABLE:
			m_bIsBtAlreadyActive = TRUE;
			break;

		default:
			m_bIsBtAlreadyActive = FALSE;

			iErr = BthSetMode(BTH_CONNECTABLE);

			if (iErr != ERROR_SUCCESS) {
				DBG_TRACE_INT(L"Debug - WindowsBlueTooth.cpp - ActivateBT() FAILED [1] ", 5, TRUE, iErr);
				return FALSE;
			}

			break;
	}

	DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - ActivateBT() OK\n", 5, FALSE);
	return TRUE;
}

BOOL WindowsBlueTooth::DeActivateBT() {
	DWORD dwRadioMode = 0;
	INT iErr;

	iErr = BthGetMode(&dwRadioMode);

	if (dwRadioMode == BTH_POWER_OFF) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - DeActivateBT() OK\n", 5, FALSE);
		return TRUE;
	}

	if (m_bIsBtAlreadyActive) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - DeActivateBT() OK\n", 5, FALSE);
		return TRUE;
	}

	iErr = BthSetMode(BTH_POWER_OFF);
	iErr = BthGetMode(&dwRadioMode);

	if (dwRadioMode != BTH_POWER_OFF) {
		DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - DeActivateBT() FAILED ", 5, TRUE);
		return FALSE;
	}

	DBG_TRACE(L"Debug - WindowsBlueTooth.cpp - DeActivateBT() OK\n", 5, FALSE);
	return TRUE;
}