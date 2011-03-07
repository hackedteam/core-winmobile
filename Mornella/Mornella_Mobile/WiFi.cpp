#include "WiFi.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
CWifi::CWifi()
{
	m_hNDUIO=NULL;
	InitializeCriticalSection(&m_Lock);
	OpenDriver();
}

//////////////////////////////////////////////////////////////////////////
CWifi::~CWifi()
{
	if(m_hNDUIO != NULL)
	{
		CloseDriver();
	}
	DeleteCriticalSection(&m_Lock);
}

//////////////////////////////////////////////////////////////////////////
//get list of usable adapters
BOOL CWifi::GetAdapters(wstring &pDest) {
	HANDLE hFile;
	BYTE Buffer[2048];
	void *pvBuf;
	WCHAR TmpBuf[1024];
	WCHAR *pszOut;
	DWORD dwRet;
	BOOL retval;

	EnterCriticalSection(&m_Lock);
	retval = FALSE;

	//open NDIS driver
	hFile = CreateFile(L"NDS0:", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
	
	if (hFile == INVALID_HANDLE_VALUE) {
		LeaveCriticalSection(&m_Lock);
		return retval;
	}

	pvBuf = (void *)(&Buffer[0]);
	dwRet = sizeof(Buffer);

	if (DeviceIoControl(hFile, IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, pvBuf, sizeof(Buffer), &dwRet, NULL)) {
		//adapter list ok.
		LPWSTR pszStr;
		dwRet = 0;
		pszOut = TmpBuf;

		//no string classes used, so no MFC or ATL dependency.
		for (pszStr = (LPWSTR)pvBuf; *pszStr; pszStr += wcslen(pszStr) + 1) {
			//check if adapter name is ok, skip infrared, gprs, ActiveSync etc.
			if (wcsicmp(pszStr, L"ASYNCMAC1") && wcsicmp(pszStr, L"IRSIR1") &&
				wcsicmp(pszStr, L"L2TP1") && wcsicmp(pszStr, L"PPTP1") &&
				wcsicmp(pszStr, L"RNDISFN1") && wcsicmp(pszStr, L"WWAN1") &&
				wcsicmp(pszStr, L"XSC1_IRDA1")) {						
				//not the first adapter?
				if (pszOut != TmpBuf) {
					//append separator
					wcscat(pszOut, L",");
					pszOut++;
					dwRet += sizeof(WCHAR);
				}

				wcscpy(pszOut, pszStr);
				pszOut += wcslen(pszStr);
				dwRet += sizeof(WCHAR) * wcslen(pszStr);
			}
		}

		pDest = TmpBuf;
		retval = TRUE;
	}

	CloseHandle(hFile);
	LeaveCriticalSection(&m_Lock);
	return retval;
}

//////////////////////////////////////////////////////////////////////////
//open the NDISUIO driver
BOOL CWifi::OpenDriver()
{
HANDLE hDev;

	//already opened?
	if(m_hNDUIO != NULL)
	{
		return TRUE;
	}

	hDev=CreateFile(NDISUIO_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, INVALID_HANDLE_VALUE);
	if(hDev == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	else
	{
		m_hNDUIO=hDev;
		return TRUE;
	}
}

//////////////////////////////////////////////////////////////////////////
//at the end, close NDISUIO handle
void CWifi::CloseDriver()
{
	CloseHandle(m_hNDUIO);
	m_hNDUIO=NULL;
}

//////////////////////////////////////////////////////////////////////////
//initiate station scanning
BOOL CWifi::RefreshBSSIDs(wstring &pAdapter)
{
	NDISUIO_SET_OID nso;
	DWORD dwBytesRet;
	BOOL retval;

	EnterCriticalSection(&m_Lock);
	nso.ptcDeviceName = (WCHAR *)pAdapter.c_str();
	nso.Oid = OID_802_11_BSSID_LIST_SCAN;

	if (pAdapter.empty()) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - RefreshBSSIDs() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	dwBytesRet = 0;

	if (!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)&nso, sizeof(NDISUIO_SET_OID), NULL, 0, &dwBytesRet, NULL)) {
		retval = FALSE;
		DBG_TRACE(L"Debug - WiFi.cpp - RefreshBSSIDs() FAILED [1]\n", 4, FALSE);
	} else {
		Sleep(6000); // Diamo tempo alla scheda di effettuare lo scan!!!
		retval = TRUE;
		DBG_TRACE(L"Debug - WiFi.cpp - RefreshBSSIDs() OK\n", 6, FALSE);
	}

	LeaveCriticalSection(&m_Lock);
	return retval;
}

//////////////////////////////////////////////////////////////////////////
//get a list of currently visible stations
BOOL CWifi::GetBSSIDs(wstring &pAdapter, struct BSSIDInfo *pDest, DWORD dwBufSizeBytes, DWORD &dwReturnedItems) {
	PNDISUIO_QUERY_OID pNQO;
	DWORD dwBytesRet;
	BYTE *pBuffer, *pByte;
	PNDIS_802_11_BSSID_LIST pList;
	UINT i;

	if (pDest == NULL || pAdapter.empty()) {
		dwReturnedItems = 0;
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSIDs() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);
	pBuffer = new(std::nothrow) BYTE[20000];

	if (pBuffer == NULL) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSIDs() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	pNQO = (PNDISUIO_QUERY_OID)pBuffer;

	pNQO->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	pNQO->Oid = OID_802_11_BSSID_LIST;

	// Run query
	dwBytesRet = 0;

	if (!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_QUERY_OID_VALUE, (void *)pNQO, 18192, (void *)pNQO, 18192, &dwBytesRet, NULL)) {
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSIDs() FAILED [2] ", 4, TRUE);
		delete[] pBuffer;
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	pList = (PNDIS_802_11_BSSID_LIST)&pNQO->Data;

	// Controlliamo l'allineamento
	if ((DWORD)pList & 3) {
		delete[] pBuffer;
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSIDs() FAILED [3]\n", 4, FALSE);
		return FALSE;
	}

	dwReturnedItems = pList->NumberOfItems;

	//first item in array
	PNDIS_WLAN_BSSID pItem = pList->Bssid;

	for (i = 0; i < dwReturnedItems; i++) {
		if (dwReturnedItems > dwBufSizeBytes / sizeof(BSSIDInfo)) {
			break;
		}

		if ((DWORD)pItem & 3) {
			dwReturnedItems = i;
			break;
		}

		CopyMemory(pDest[i].BSSID, pItem->MacAddress, sizeof(pItem->MacAddress));
		mbstowcs(pDest[i].SSID, (char *)pItem->Ssid.Ssid, sizeof(pItem->Ssid.Ssid));
		pDest[i].RSSI = pItem->Rssi;

		if (pItem->Configuration.DSConfig > 14) {
			pDest[i].Channel = (pItem->Configuration.DSConfig - 2407000) / 5000;
		} else {
			pDest[i].Channel = pItem->Configuration.DSConfig;
		}

		pDest[i].Auth = pItem->Privacy;
		pDest[i].Infastructure = pItem->InfrastructureMode;

		//some pointer magic...actually pItem->Length was not sizeof(NDIS_WLAN_BSSID)
		//so use returned length
		pByte = (PBYTE)pItem;
		pByte += pItem->Length;
		pItem = (PNDIS_WLAN_BSSID)pByte;
	} //for

	delete[] pBuffer;
	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - GetBSSIDs() OK\n", 6, FALSE);
	return TRUE;
}

BOOL CWifi::SetAdHocConnection(wstring &pAdapter, LPSTR pSsid) {
	DWORD dwBytesRet;
	UCHAR SetBuffer[sizeof(NDISUIO_SET_OID) - sizeof(ULONG) + sizeof(NDIS_802_11_SSID)];
	PNDISUIO_SET_OID pSetOid;

	if (pAdapter.empty() || pSsid == NULL || strlen(pSsid) == 0) {
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);

	ZeroMemory(SetBuffer, sizeof(SetBuffer));
	pSetOid = (PNDISUIO_SET_OID)&SetBuffer[0];
	pSetOid->Oid = OID_802_11_SSID;

	PNDIS_802_11_SSID psSSID = (PNDIS_802_11_SSID)pSetOid->Data;
	psSSID->SsidLength = strlen((char *)pSsid);

	// L'SSID al massimo puo' essere di 31 byte
	if (psSSID->SsidLength >= 32)
		psSSID->SsidLength = 31;

	CopyMemory(psSSID->Ssid, pSsid, psSSID->SsidLength);	

	// Run query
	dwBytesRet = 0;
	if (!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)SetBuffer, sizeof(SetBuffer), 
						NULL, 0, &dwBytesRet, NULL)) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}
/*
	pList=(PNDIS_802_11_BSSID_LIST)&pNQO->Data;
	dwReturnedItems=pList->NumberOfItems;

	//first item in array
	PNDIS_WLAN_BSSID pItem=pList->Bssid;

	for(i=0; i<pList->NumberOfItems; i++)
	{
		memcpy(pDest[i].BSSID, pItem->MacAddress, sizeof(pItem->MacAddress));
		mbstowcs(pDest[i].SSID, (char *)pItem->Ssid.Ssid, sizeof(pItem->Ssid.Ssid));
		pDest[i].RSSI=pItem->Rssi;
		if(pItem->Configuration.DSConfig > 14)
		{
			pDest[i].Channel=(pItem->Configuration.DSConfig - 2407000) / 5000;
		}
		else
		{
			pDest[i].Channel=pItem->Configuration.DSConfig;
		}
		pDest[i].Auth=pItem->Privacy;
		pDest[i].Infastructure=pItem->InfrastructureMode;

		//some pointer magic...actually pItem->Length was not sizeof(NDIS_WLAN_BSSID)
		//so use returned length
		pByte=(BYTE *)pItem;
		pByte+=pItem->Length;
		pItem=(PNDIS_WLAN_BSSID)pByte;
	}//for
*/
	LeaveCriticalSection(&m_Lock);
	return TRUE;
}

// Non serve chiamare questa funzione se usiamo la WZCConfigureNetwork()
BOOL CWifi::SetConnectionMode(wstring &pAdapter, NDIS_802_11_NETWORK_INFRASTRUCTURE iMode, NDIS_802_11_AUTHENTICATION_MODE aMode) {
	NDISUIO_SET_OID sOid;
	DWORD dwBytesReturned;

	if(pAdapter.empty()) {
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);

	sOid.ptcDeviceName = (WCHAR *)pAdapter.c_str();
	sOid.Oid = OID_802_11_AUTHENTICATION_MODE;
	CopyMemory(&sOid.Data, &aMode, sizeof(NDIS_802_11_AUTHENTICATION_MODE));

	// Settiamo l'authentication mode
	dwBytesReturned = 0;

	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)&sOid, sizeof(NDISUIO_SET_OID), NULL, 
						0, &dwBytesReturned, NULL)) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}
			
	// E poi il tipo di infrastruttura
	sOid.Oid = OID_802_11_INFRASTRUCTURE_MODE;  
	CopyMemory(&sOid.Data, &iMode, sizeof(NDIS_802_11_NETWORK_INFRASTRUCTURE));  

	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)&sOid, sizeof(NDISUIO_SET_OID), NULL, 
						0, &dwBytesReturned, NULL)) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	} 

	return TRUE;
}

// Non serve chiamare questa funzione se usiamo la WZCConfigureNetwork()
BOOL CWifi::SetSsid(wstring &pAdapter, LPCSTR pSsid) {
	BYTE SetBuffer[sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_SSID)];
	PNDISUIO_SET_OID psOid;
	NDIS_802_11_SSID ssid;
	DWORD dwBytesReturned, dwInFlags;

	if(pAdapter.empty() || pSsid == NULL || strlen(pSsid) == 0) {
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);
	ZeroMemory(&SetBuffer, sizeof(SetBuffer));

	psOid = (PNDISUIO_SET_OID)&SetBuffer;
	psOid->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	psOid->Oid = OID_802_11_SSID;

	ssid.SsidLength = strlen(pSsid);

	if(ssid.SsidLength > 32)
		ssid.SsidLength = 32;

	CopyMemory(ssid.Ssid, pSsid, ssid.SsidLength);
	CopyMemory(psOid->Data, &ssid, sizeof(NDIS_802_11_SSID));

	// Impostiamo l'SSID
	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)psOid, sizeof(SetBuffer), NULL, 
		0, &dwBytesReturned, NULL)) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	INTF_ENTRY Intf;
	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = (WCHAR *)pAdapter.c_str();
	DWORD status = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	LeaveCriticalSection(&m_Lock);
	return TRUE;
}

BOOL CWifi::Disassociate(wstring &pAdapter) {
	INTF_ENTRY ientry;
	BYTE SetBuffer[sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_SSID)];
	DWORD dwBytesReturned;
	PNDISUIO_SET_OID psOid;

	EnterCriticalSection(&m_Lock);

	ZeroMemory(&ientry, sizeof(ientry));
	ZeroMemory(&SetBuffer, sizeof(SetBuffer));

	ientry.wszGuid = (WCHAR *)pAdapter.c_str();

	if(WZCSetInterface(NULL, INTFCTL_FALLBACK, &ientry, NULL) != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	//WZCDeleteIntfObj(&ientry);

	psOid = (PNDISUIO_SET_OID)&SetBuffer;
	psOid->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	psOid->Oid = OID_802_11_DISASSOCIATE;

	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)psOid, sizeof(SetBuffer), NULL, 
		0, &dwBytesReturned, NULL)) {
			LeaveCriticalSection(&m_Lock);
			return FALSE;
	}
	
	LeaveCriticalSection(&m_Lock);
	return TRUE;
}

BOOL CWifi::GetBSSID(wstring &pAdapter, LPWSTR pSsid, struct BSSIDInfo *pDest) {
	BSSIDInfo bi[20];
	DWORD len = sizeof(bi);
	DWORD retr, i;

	ZeroMemory(bi, sizeof(bi));

	if (pAdapter.empty() || pDest == NULL) {
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSID() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	if (RefreshBSSIDs(pAdapter) == FALSE) {
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSID() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (GetBSSIDs(pAdapter, bi, len, retr) == FALSE) {
		DBG_TRACE(L"Debug - WiFi.cpp - GetBSSID() FAILED [2]\n", 4, FALSE);
		return FALSE;
	}

	for (i = 0; i < retr; i++) {
		if (RtlEqualMemory(bi[i].SSID, pSsid, WideLen(pSsid))) {
			CopyMemory(pDest, &bi[i], sizeof(BSSIDInfo));
			DBG_TRACE(L"Debug - WiFi.cpp - GetBSSID() OK\n", 6, FALSE);
			return TRUE;
		}
	}

	DBG_TRACE(L"Debug - WiFi.cpp - GetBSSID() FAILED [3]\n", 4, FALSE);
	return FALSE;
}

// Non serve chiamare questa funzione se usiamo la WZCConfigureNetwork()
BOOL CWifi::SetEncryptionKey(wstring &pAdapter, BYTE* pKey, UINT uKeyLen) {
	PNDIS_802_11_WEP pWepKey = NULL;
	NDIS_802_11_ENCRYPTION_STATUS NdisEncryption;
	BYTE SetStatusBuffer[sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_ENCRYPTION_STATUS)];
	PNDISUIO_SET_OID psOid, psKeyOid = NULL;
	DWORD dwBytesReturned;

	if(pAdapter.empty() || uKeyLen > 32)
		return FALSE;

	EnterCriticalSection(&m_Lock);

	// 1. Impostiamo il tipo di cifratura
	ZeroMemory(&SetStatusBuffer, sizeof(SetStatusBuffer));

	psOid = (PNDISUIO_SET_OID)&SetStatusBuffer;
	psOid->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	psOid->Oid = OID_802_11_ENCRYPTION_STATUS;

	NdisEncryption = Ndis802_11Encryption1Enabled;
	CopyMemory(psOid->Data, &NdisEncryption, sizeof(NDIS_802_11_ENCRYPTION_STATUS));

	// Settiamo l'encryption status
	dwBytesReturned=0;
	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)psOid, sizeof(SetStatusBuffer), NULL, 
						0, &dwBytesReturned, NULL)) {

		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	// 2. Impostiamo la chiave
	psKeyOid = (PNDISUIO_SET_OID) new(std::nothrow) BYTE[sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_WEP) + uKeyLen];

	if(psKeyOid == NULL) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	ZeroMemory(psKeyOid, sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_WEP) + uKeyLen);

	// Inizializza la struttura dell'OID
	psKeyOid->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	psKeyOid->Oid = OID_802_11_ADD_WEP;

	// Puntiamo al membro Data[] della struttura
	pWepKey = (PNDIS_802_11_WEP)((BYTE *)psKeyOid + sizeof(NDISUIO_SET_OID) - sizeof(psKeyOid->Data));

	// Calcola la lunghezza della struttura
	pWepKey->Length = FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial) + uKeyLen;

	pWepKey->KeyIndex = 0x80000000;
	pWepKey->KeyLength = uKeyLen;

	// Copia la chiave nella struttura
	CopyMemory(pWepKey->KeyMaterial, pKey, pWepKey->KeyLength);

	if (!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (LPVOID)psKeyOid, sizeof(NDISUIO_SET_OID) + sizeof(NDIS_802_11_WEP) + uKeyLen,
						NULL, 0, &dwBytesReturned, NULL)) {

		delete[] psKeyOid;
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	delete[] psKeyOid;

	// 3. Abilitiamo il WEP
	ZeroMemory(&SetStatusBuffer, sizeof(SetStatusBuffer));

	psOid = (PNDISUIO_SET_OID)&SetStatusBuffer;
	psOid->ptcDeviceName = (WCHAR *)pAdapter.c_str();
	psOid->Oid = OID_802_11_WEP_STATUS;

	NdisEncryption = Ndis802_11WEPEnabled;
	CopyMemory(psOid->Data, &NdisEncryption, sizeof(NDIS_802_11_ENCRYPTION_STATUS));

	if(!DeviceIoControl(m_hNDUIO, IOCTL_NDISUIO_SET_OID_VALUE, (void *)psOid, sizeof(SetStatusBuffer), NULL, 
						0, &dwBytesReturned, NULL)) {

		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	LeaveCriticalSection(&m_Lock);
	return TRUE;
}

BOOL CWifi::WZCCheckAssociation(wstring &pAdapterName) {
	DWORD dwStatus, dwInFlags;
	INTF_ENTRY Intf;

	if (pAdapterName.size() == 0) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);
	ZeroMemory(&Intf, sizeof(INTF_ENTRY));

	// Vediamo prima se la scheda e' supportata (la Query vuole il nome senza la GUID....)
	Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

	dwStatus = WZCQueryInterface(NULL, INTF_ALL, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if((Intf.dwCtlFlags & INTF_OIDSSUPP) != INTF_OIDSSUPP) {
		//WZCDeleteIntfObj(&Intf);
		//LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() FAILED [2]\n", 4, FALSE);
		//return FALSE;
	}

	WZCDeleteIntfObj(&Intf);

	dwStatus = WZCQueryInterface(NULL, INTF_BSSID, &Intf, &dwInFlags);
	
	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() FAILED [3]\n", 4, FALSE);
		return FALSE;
	}

	// Qui dentro c'e' il MAC a cui siamo associati
	if ((dwInFlags & INTF_BSSID) == INTF_BSSID) {
		BYTE *pMac = (BYTE *)Intf.rdBSSID.pData;
		BYTE NullMac[6];

		ZeroMemory(NullMac, 6);

		if (pMac && !RtlEqualMemory(pMac, NullMac, 6)) {
			WZCDeleteIntfObj(&Intf);
			LeaveCriticalSection(&m_Lock);
			DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() OK\n", 5, FALSE);
			return TRUE;
		}
	}

	WZCDeleteIntfObj(&Intf);
	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - WZCCheckAssociation() FAILED [4]\n", 4, FALSE);
	return FALSE;
}

BOOL CWifi::WZCConfigureNetwork(wstring &pAdapter, struct BSSIDInfo *pBssid, BYTE* pKey, UINT uKeyLen) {
	DWORD dwInFlags = 0, dwStatus;
	INTF_ENTRY Intf;
	WZC_WLAN_CONFIG wzcConfig;
	UINT i;
	WZC_802_11_CONFIG_LIST *pNewConfigList = NULL;

	if (pAdapter.empty() || pBssid == NULL || uKeyLen > 32) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);

	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	ZeroMemory(&wzcConfig, sizeof(wzcConfig));
	Intf.wszGuid = (WCHAR *)pAdapter.c_str();

	// Vediamo prima se la scheda e' supportata (la Query vuole il nome senza la GUID....)
	dwStatus = WZCQueryInterface(NULL, INTF_ALL, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if ((Intf.dwCtlFlags & INTF_OIDSSUPP) != INTF_OIDSSUPP) {
		//WZCDeleteIntfObj(&Intf);
		//LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [2]\n", 4, FALSE);
		//return FALSE;
	}

	// TEST
	wzcConfig.Length = sizeof(wzcConfig);
	wzcConfig.dwCtlFlags = WZCCTL_VOLATILE;

	// Ssid (in char)
	wzcConfig.Ssid.SsidLength = wcslen(pBssid->SSID);

	for (i = 0; i < wzcConfig.Ssid.SsidLength; i++)
		wzcConfig.Ssid.Ssid[i] = (char)pBssid->SSID[i];

	wzcConfig.InfrastructureMode = Ndis802_11IBSS;
	wzcConfig.AuthenticationMode = Ndis802_11AuthModeOpen;
	wzcConfig.KeyIndex = 0;       // 0 is the per-client key, 1->N are the global keys
	wzcConfig.KeyLength = 0;      // length of key in bytes
	wzcConfig.Privacy = Ndis802_11WEPDisabled;

	DWORD dwOutFlags, dwError;
	WZC_802_11_CONFIG_LIST *pConfigList = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;

 	if (!pConfigList) { // La lista dei preferiti e' vuota
		DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
		pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);

		pNewConfigList->NumberOfItems = 1;
		pNewConfigList->Index = 0;
		CopyMemory(pNewConfigList->Config, &wzcConfig, sizeof(WZC_WLAN_CONFIG));

		Intf.rdStSSIDList.pData = (BYTE *)pNewConfigList;
		Intf.rdStSSIDList.dwDataLen = dwDataLen;

		LocalFree(pConfigList);
		pConfigList = NULL;
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() [Preferred list empty]\n", 6, FALSE);
	} else {
		ULONG uiNumberOfItems = pConfigList->NumberOfItems;

		// Vediamo se questa rete e' gia' nei preferiti
		for (i = 0; i < uiNumberOfItems; i++) {
			if (RtlEqualMemory(&wzcConfig.Ssid, &pConfigList->Config[i].Ssid, sizeof(NDIS_802_11_SSID))) {
				pConfigList->Index = i;

				dwError = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwOutFlags);

				if (dwError != ERROR_SUCCESS) {
					__try {
						WZCDeleteIntfObj(&Intf);
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [0]\n", 3, FALSE);
					}

					LeaveCriticalSection(&m_Lock);
					DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [3]\n", 4, FALSE);
					return FALSE;
				}

				__try {
					WZCDeleteIntfObj(&Intf);
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [1]\n", 3, FALSE);
				}
					
				LeaveCriticalSection(&m_Lock);
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() OK\n", 6, FALSE);
				return TRUE;
			}
		}

		uiNumberOfItems += 1;

		DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST) + uiNumberOfItems * sizeof(WZC_WLAN_CONFIG);
		pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);

		if (pNewConfigList == NULL) {
			__try {
				WZCDeleteIntfObj(&Intf);
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [2]\n", 3, FALSE);
			}

			LeaveCriticalSection(&m_Lock);
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [4]\n", 4, FALSE);
			return FALSE;
		}

		pNewConfigList->NumberOfItems = uiNumberOfItems;
		pNewConfigList->Index = uiNumberOfItems - 1;

		if (pConfigList->NumberOfItems) {
			// Copiamo i vecchi oggetti nella lista
			CopyMemory(pNewConfigList->Config, pConfigList->Config, (uiNumberOfItems - 1) * sizeof(WZC_WLAN_CONFIG));
		}

		if (pConfigList) {
			__try {
				LocalFree(pConfigList);
				pConfigList = NULL;
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [3]\n", 3, FALSE);
			}
		}

		CopyMemory(pNewConfigList->Config + uiNumberOfItems - 1, &wzcConfig, sizeof(wzcConfig));
		Intf.rdStSSIDList.pData = (BYTE *)pNewConfigList;
		Intf.rdStSSIDList.dwDataLen = dwDataLen;
	}

	dwError = WZCSetInterface(NULL, /*INTF_ALL_FLAGS |*/ INTF_PREFLIST, &Intf, &dwOutFlags);

	__try {
		WZCDeleteIntfObj(&Intf);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [4]\n", 3, FALSE);
	}

	if (pNewConfigList) {
		__try {
			LocalFree(pNewConfigList);
			pNewConfigList = NULL;
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() EXCEPTION [5]\n", 3, FALSE);
		}
	}

	if (dwError != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() FAILED [5]\n", 4, FALSE);
		return FALSE;
	}

	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - WZCConfigureNetwork() OK\n", 6, FALSE);
	return TRUE;

	// FINE TEST

	/*WZCDeleteIntfObj(&Intf);

	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = pAdapter;

	wzcConfig.Length = sizeof(wzcConfig);

	// dwCtlFlags
	wzcConfig.dwCtlFlags = WZCCTL_VOLATILE; // | WZCCTL_WEPK_PRESENT | WZCCTL_WEPK_XFORMAT;

	wzcConfig.NetworkTypeInUse = Ndis802_11FH;

	// Impostiamo il BSSID 
	//memcpy(wzcConfig.MacAddress, pBssid->BSSID, 6);

	// Ssid (in char)
	wzcConfig.Ssid.SsidLength = wcslen(pBssid->SSID);
	
	for(i = 0; i < wzcConfig.Ssid.SsidLength; i++)
		wzcConfig.Ssid.Ssid[i] = (char)pBssid->SSID[i];

	// Privacy
	wzcConfig.Privacy = Ndis802_11EncryptionDisabled; //Ndis802_11Encryption1Enabled; 

	// adhoc? or infrastructure net?
	wzcConfig.InfrastructureMode = Ndis802_11IBSS;

	// Key Index
	//wzcConfig.KeyIndex = 0x80000000;

	// Lunghezza della chiave
	//wzcConfig.KeyLength = uKeyLen;

	// Impostiamo la chiave (disabilitato, per ora niente WEP/WPA)
	//memcpy(wzcConfig.KeyMaterial, pKey, uKeyLen);

	// Authentication
	wzcConfig.AuthenticationMode = Ndis802_11AuthModeOpen;

	DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
	WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *) new(std::nothrow) BYTE[dwDataLen];

	if(pNewConfigList == NULL) {
		WZCDeleteIntfObj(&Intf);
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	pNewConfigList->NumberOfItems = 1;
	pNewConfigList->Index = 0;

	CopyMemory(pNewConfigList->Config, &wzcConfig, sizeof(wzcConfig));
	Intf.rdStSSIDList.pData = (BYTE*)pNewConfigList;
	Intf.rdStSSIDList.dwDataLen = dwDataLen;

	dwStatus = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	// wzcGuid non viene liberato dalla WZCDeleteIntfObj(), ma noi cmq
	// la passiamo come parametro della funzione
	WZCDeleteIntfObj(&Intf);
	delete[] pNewConfigList;

	if(dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	LeaveCriticalSection(&m_Lock);
	return TRUE;*/
}

BOOL CWifi::WZCConnectToOpen(wstring &pAdapterName) {
	DWORD dwInFlags = 0, dwOutFlags, dwStatus;
	INTF_ENTRY Intf;
	BSSIDInfo bi[30];
	DWORD len = sizeof(bi), retr;
	BOOL bFound = FALSE;
	UINT i, j;

	if (pAdapterName.size() == 0) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [No adapter name received]\n", 4, FALSE);
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);

	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

	dwStatus = WZCQueryInterface(NULL, INTF_ALL, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [Cannot perform WZCQueryInterface]\n", 4, FALSE);
		return FALSE;
	}

	if ((Intf.dwCtlFlags & INTF_OIDSSUPP) != INTF_OIDSSUPP) {
		//WZCDeleteIntfObj(&Intf);
		//LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [OID Unsupported] ", 4, FALSE);
		DBG_TRACE_INT(L"value: ", 4, FALSE, Intf.dwCtlFlags);
		//return FALSE;
	}

	WZCDeleteIntfObj(&Intf);

	// Vediamo la lista delle reti presenti
	// Dovremmo usare WZCQueryInterface() per leggere le reti disponibili, ma su WM6
	// non si comporta come dovrebbe, quindi ci interfacciamo al driver...
	if (RefreshBSSIDs(pAdapterName) == FALSE) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [RefreshBSSIDs() failed]\n", 4, FALSE);
		return FALSE;
	}

	ZeroMemory(bi, sizeof(bi));

	if (GetBSSIDs(pAdapterName, bi, len, retr) == FALSE) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [GetBSSIDs() failed]\n", 4, FALSE);
		return FALSE;
	}

	// Prendiamo la lista delle reti preferite
	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

	dwStatus = WZCQueryInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [Cannot perform 2nd WZCQueryInterface()]\n", 4, FALSE);
		return FALSE;
	}

	WZC_802_11_CONFIG_LIST *pConf = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;

	// Proviamo a collegarci ad una rete aperta e non configurata.
	for (i = 0; i < retr; i++) {
		WZC_WLAN_CONFIG *p;
		WCHAR wPrefSsid[32];

		bFound = FALSE;

		// Se la rete non e' aperta o e' ad-hoc saltala
		if (bi[i].Auth || bi[i].Infastructure == 0)
			continue;

		if (pConf && pConf->NumberOfItems) {
			for (p = pConf->Config, j = 0; j < pConf->NumberOfItems; j++, p++) {
				wsprintf(wPrefSsid, L"%S", p->Ssid.Ssid);

				// Vediamo se la rete e' gia' tra i preferiti
				if (RtlEqualMemory(wPrefSsid, bi[i].SSID, p->Ssid.SsidLength * sizeof(WCHAR)))
					bFound = TRUE;
			}
		}

		// Se la rete e' tra le preferite saltiamola
		if (bFound)
			continue;

		// Altrimenti ci colleghiamo
		WZC_WLAN_CONFIG wzcConfig;

		ZeroMemory(&wzcConfig, sizeof(wzcConfig));
		Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

		wzcConfig.Length = sizeof(wzcConfig);

		// dwCtlFlags
		wzcConfig.dwCtlFlags = WZCCTL_VOLATILE;
		wzcConfig.NetworkTypeInUse = Ndis802_11FH;
		wzcConfig.Ssid.SsidLength = wcslen(bi[i].SSID);
		CopyMemory(wzcConfig.MacAddress, bi[i].BSSID, 6);
		sprintf((CHAR *)wzcConfig.Ssid.Ssid, "%S", bi[i].SSID);

		// Privacy
		wzcConfig.Privacy = Ndis802_11EncryptionDisabled; //Ndis802_11Encryption1Enabled; 

		// adhoc? or infrastructure net?
		wzcConfig.InfrastructureMode = Ndis802_11Infrastructure;

		// Authentication
		wzcConfig.AuthenticationMode = Ndis802_11AuthModeOpen;

		DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
		WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);

		if (pNewConfigList == NULL) {
			__try {
				WZCDeleteIntfObj(&Intf);
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() EXCEPTION [0]\n", 3, FALSE);
			}

			LeaveCriticalSection(&m_Lock);
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [Cannot allocate memory]\n", 4, FALSE);
			return FALSE;
		}

		pNewConfigList->NumberOfItems = 1;
		pNewConfigList->Index = 0;

		CopyMemory(pNewConfigList->Config, &wzcConfig, sizeof(wzcConfig));

		// E' undoc ma questo membro, e gli altri di Intf vengono allocati con LocalAlloc()
		if (Intf.rdStSSIDList.pData) {
			__try {
				LocalFree(Intf.rdStSSIDList.pData);
				Intf.rdStSSIDList.pData = NULL;
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() EXCEPTION [1]\n", 3, FALSE);
			}
		}

		Intf.rdStSSIDList.pData = (BYTE *)pNewConfigList;
		Intf.rdStSSIDList.dwDataLen = dwDataLen;

		//dwStatus = WZCSetInterface(NULL, INTF_SSID, &Intf, &dwOutFlags);   
		dwStatus = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwOutFlags);

		if (pNewConfigList) {
			__try {
				LocalFree(pNewConfigList);
				pNewConfigList = NULL;
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() EXCEPTION [2]\n", 3, FALSE);
			}
		}

		if (dwStatus != ERROR_SUCCESS) {
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [Cannot perform WZCSetInterface()] ", 3, TRUE);
			continue;
		}

		Sleep(10000);

		// Se WZC decide che la rete a cui siamo connessi e' piu' sicura di quella
		// alla quale vorremmo connetterci... Non possiamo proprio farci niente.
		// XXX Possiamo provare a impostare artificialmente tutte le reti a sicurezza minima.
		if (WZCCheckAssociation(pAdapterName) == TRUE) {
			__try {
				WZCDeleteIntfObj(&Intf);
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() EXCEPTION [2]\n", 3, FALSE);
			}

			LeaveCriticalSection(&m_Lock);
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() OK\n", 6, FALSE);
			return TRUE;
		}
	}

	// XXXX Questa delete andrebbe fatta solo se siamo entrati e poi usciti dal for(), da sistemare!
	__try {
		WZCDeleteIntfObj(&Intf);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() EXCEPTION [3]\n", 3, FALSE);
	}

	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToOpen() FAILED [Cannot find a suitable network]\n", 4, FALSE);
	return FALSE;
}

BOOL CWifi::WZCConnectToPreferred(wstring &pAdapterName) {
	DWORD dwInFlags = 0, dwStatus;
	INTF_ENTRY Intf;
	BSSIDInfo bi[30];
	DWORD len = sizeof(bi), retr;
	BOOL bFound = FALSE;
	UINT i, j;

	if (pAdapterName.size() == 0) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [No adapter name received]\n", 4, FALSE);
		return FALSE;
	}

	EnterCriticalSection(&m_Lock);

	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

	dwStatus = WZCQueryInterface(NULL, INTF_ALL, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [Cannot perform WZCQueryInterface]\n", 4, FALSE);
		return FALSE;
	}

	if ((Intf.dwCtlFlags & INTF_OIDSSUPP) != INTF_OIDSSUPP) {
		//WZCDeleteIntfObj(&Intf);
		//LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [WiFi OID Unsupported] ", 4, FALSE);
		DBG_TRACE_INT(L"value: ", 4, FALSE, Intf.dwCtlFlags);
		//return FALSE;
	}

	WZCDeleteIntfObj(&Intf);

	// Vediamo la lista delle reti presenti
	// Dovremmo usare WZCQueryInterface() per leggere le reti disponibili, ma su WM6
	// non si comporta come dovrebbe, quindi ci interfacciamo al driver...
	if (RefreshBSSIDs(pAdapterName) == FALSE) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [RefreshBSSIDs() failed]\n", 4, FALSE);
		return FALSE;
	}

	ZeroMemory(bi, sizeof(bi));

	if (GetBSSIDs(pAdapterName, bi, len, retr) == FALSE) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [GetBSSIDs() failed]\n", 4, FALSE);
		return FALSE;
	}

	// Prendiamo la lista delle reti preferite
	ZeroMemory(&Intf, sizeof(INTF_ENTRY));
	Intf.wszGuid = (WCHAR *)pAdapterName.c_str();

	dwStatus = WZCQueryInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [Cannot perform 2nd WZCQueryInterface]\n", 4, FALSE);
		return FALSE;
	}

	WZC_802_11_CONFIG_LIST *pConf = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;
	
	// Scorriamo le reti trovate e vediamo se qualcuna e' nella lista delle preferite
	if (!pConf || pConf->NumberOfItems == 0) {
		__try {
			WZCDeleteIntfObj(&Intf);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() EXCEPTION [0]\n", 3, FALSE);
		}

		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [No network found]\n", 4, FALSE);
		return FALSE;
	}

	for (i = 0; i < retr; i++) {
		WZC_WLAN_CONFIG *p;
		WCHAR wPrefSsid[32];
		WZC_WLAN_CONFIG wlanTmp;

		for (p = pConf->Config, j = 0; j < pConf->NumberOfItems; j++, p++) {
			if (p->Ssid.SsidLength != wcslen(bi[i].SSID))
				continue;

			wsprintf(wPrefSsid, L"%S", p->Ssid.Ssid);

			if (RtlEqualMemory(wPrefSsid, bi[i].SSID, p->Ssid.SsidLength * sizeof(WCHAR))) {
				/* // Disconnettiamoci...
				UINT old = pConf->NumberOfItems;
				pConf->NumberOfItems = 0; // Facciamogli credere che non ci sono reti preferite
				dwStatus = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);*/

				// Swappiamo in testa alla lista la rete alla quale ci vogliamo connettere
				CopyMemory(&wlanTmp, &pConf->Config[0], sizeof(WZC_WLAN_CONFIG));
				CopyMemory(&pConf->Config[0], &pConf->Config[j], sizeof(WZC_WLAN_CONFIG));
				CopyMemory(&pConf->Config[j], &wlanTmp, sizeof(WZC_WLAN_CONFIG));

				//pConf->NumberOfItems = old; // Restoriamo le reti preferite
				dwStatus = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);
				
				if (dwStatus != ERROR_SUCCESS)
					continue;

				Sleep(12000);

				// Se WZC decide che la rete a cui siamo connessi e' piu' sicura di quella
				// alla quale vorremmo connetterci... Non possiamo proprio farci niente.
				// XXX Possiamo provare a impostare artificialmente tutte le reti a sicurezza minima.
				if (WZCCheckAssociation(pAdapterName) == TRUE) {
					__try {
						WZCDeleteIntfObj(&Intf);
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() EXCEPTION [1]\n", 3, FALSE);
					}

					LeaveCriticalSection(&m_Lock);
					DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() OK\n", 6, FALSE);
					return TRUE;
				}
			}
		}
	}

	__try {
		WZCDeleteIntfObj(&Intf);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() EXCEPTION [2]\n", 3, FALSE);
	}

	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - WZCConnectToPreferred() FAILED [Cannot find a suitable network]\n", 4, FALSE);
	return FALSE;

/*

	LeaveCriticalSection(&m_Lock);
	return FALSE;

	// Non ci sono reti preconfigurate in vista
	if(bFound == FALSE) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}


	ZeroMemory(&Intf, sizeof(INTF_ENTRY));

	DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
	WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *) new(std::nothrow) BYTE[dwDataLen];

	if(pNewConfigList == NULL) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	pNewConfigList->NumberOfItems = 1;
	pNewConfigList->Index = 0;

	// Impostiamo la rete alla quale connetterci
	WZC_WLAN_CONFIG wzcList;
	ZeroMemory(&wzcList, sizeof(wzcList));

	memcpy(pNewConfigList->Config, &wzcList, sizeof(wzcList));
	Intf.wszGuid = pAdapterName;
	Intf.rdBSSIDList.pData = (BYTE*)pNewConfigList;
	Intf.rdBSSIDList.dwDataLen = dwDataLen;

	delete[] pNewConfigList;

	// Prendiamo di nuovo la lista dei preferiti
	dwStatus = WZCQueryInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	if(dwStatus != ERROR_SUCCESS) {
		WZCDeleteIntfObj(&Intf);
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}

	// Spostiamo la rete trovata al primo posto della lista
	WZC_802_11_CONFIG_LIST *pConfigList = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;

	if(!pConfigList) { // Non ci sono reti preferite configurate, inseriamone una
		/*DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
		WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *) new(std::nothrow) BYTE[dwDataLen];

		if(pNewConfigList == NULL) {
			delete[] pNewConfigList;
			WZCDeleteIntfObj(&Intf);
			LeaveCriticalSection(&m_Lock);
			return FALSE;
		}

		pNewConfigList->NumberOfItems = 1;
		pNewConfigList->Index = 0;
		memcpy(pNewConfigList->Config, &wzcList, sizeof(wzcList));
		Intf.rdStSSIDList.pData = (BYTE*)pNewConfigList;
		Intf.rdStSSIDList.dwDataLen = dwDataLen;
	}

	// Colleghiamoci alla prima rete preconfigurata
	dwStatus = WZCSetInterface(NULL, INTF_PREFLIST, &Intf, &dwInFlags);

	WZCDeleteIntfObj(&Intf);
	delete[] pNewConfigList;

	if(dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		return FALSE;
	}*/
}

BOOL CWifi::IsWiFiPresent() {
	DWORD dwStatus;
	INTFS_KEY_TABLE IntfsTable;
	UINT i = 0;

	IntfsTable.dwNumIntfs = 0;
	IntfsTable.pIntfs = NULL;

	EnterCriticalSection(&m_Lock);

	dwStatus = WZCEnumInterfaces(NULL, &IntfsTable);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - IsWiFiPresent() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	if (!IntfsTable.dwNumIntfs) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - IsWiFiPresent() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (IntfsTable.pIntfs) {
		LocalFree(IntfsTable.pIntfs);
		IntfsTable.pIntfs = NULL;
	}

	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - IsWiFiPresent() OK\n", 6, FALSE);
	return TRUE;
}

BOOL CWifi::WZCGetAdapterName(wstring &pDest) {
	DWORD dwStatus;
	INTFS_KEY_TABLE IntfsTable;
	UINT i = 0;

	IntfsTable.dwNumIntfs = 0;
	IntfsTable.pIntfs = NULL;

	EnterCriticalSection(&m_Lock);

	dwStatus = WZCEnumInterfaces(NULL, &IntfsTable);

	if (dwStatus != ERROR_SUCCESS) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCGetAdapterName() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	if (!IntfsTable.dwNumIntfs) {
		LeaveCriticalSection(&m_Lock);
		DBG_TRACE(L"Debug - WiFi.cpp - WZCGetAdapterName() FAILED [2]\n", 4, FALSE);
		return FALSE;
	}

	pDest = IntfsTable.pIntfs[0].wszGuid;

	if (IntfsTable.pIntfs) {
		LocalFree(IntfsTable.pIntfs);
		IntfsTable.pIntfs = NULL;
	}

	LeaveCriticalSection(&m_Lock);
	DBG_TRACE(L"Debug - WiFi.cpp - WZCGetAdapterName() OK\n", 6, FALSE);
	return TRUE;
}

BOOL CWifi::GetWiFiAdapterName(wstring &pDest) {
	wstring strAdapterName;
	HKEY hWiFi;
	WCHAR wWifiKey[128];
	DWORD dwSize = sizeof(wWifiKey) / sizeof(WCHAR);

	// Proviamo ad ottenere il nome della WiFi utilizzando WZC
	if (WZCGetAdapterName(pDest) == TRUE)
		return TRUE;

	pDest = L"";

	// Non abbiamo ottenuto il nome proviamo col registro
	do {
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\WZCSVC\\Parameters\\Interfaces", 
			0, 0, &hWiFi) != ERROR_SUCCESS) {
			DBG_TRACE(L"Debug - WiFi.cpp - GetWiFiAdapterName() FAILED [0] [Cannot open key]\n", 4, FALSE);
			break;
		}

		// Prendiamo il nome solo di un adattatore
		LONG lRet = RegEnumKeyEx(hWiFi, 0, wWifiKey, &dwSize, NULL, NULL, NULL, NULL);

		RegCloseKey(hWiFi);

		if (lRet == ERROR_MORE_DATA) {
			DBG_TRACE(L"Debug - WiFi.cpp - GetWiFiAdapterName() FAILED [1] [Buffer too small]\n", 4, FALSE);
			break;
		}

		if (lRet != ERROR_SUCCESS) {
			DBG_TRACE(L"Debug - WiFi.cpp - GetWiFiAdapterName() FAILED [2] [Key not found]\n", 4, FALSE);
			break;
		}

		pDest = wWifiKey;
		return TRUE;
	} while(0);

	// Se arriviamo qui significa che la scheda non viene enumerata quando e' spenta.
	// Cerchiamo di ottenere il nome dal driver (non tutti i driver supportano le 
	// IOCTL_* definite da Microsoft)
	if (GetAdapters(strAdapterName) == TRUE) {
	// I nomi degli adapter sono separati da una "," e per ora copiamo solo il primo
		if (strAdapterName.find(L",") != wstring::npos)
			pDest = strAdapterName.substr(0, strAdapterName.find(L","));
		else
			pDest = strAdapterName;

		return TRUE;
	}

	DBG_TRACE(L"Debug - WiFi.cpp - GetWiFiAdapterName() FAILED [3] [WiFi adapter not found]\n", 4, FALSE);
	return FALSE;
}