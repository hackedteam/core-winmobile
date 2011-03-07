#include <map>
#include "PoomContactsSIMReader.h"
#include "PoomContact.h"

CPoomContactsSIMReader::CPoomContactsSIMReader()
:	IPoomFolderReader(NULL), m_hSim(NULL), m_dwTotal(0), m_dwUsed(0), m_bInit(FALSE), m_dwOSMinVersion(1)
{
	ZeroMemory(&m_simCaps, sizeof(SIMCAPS));
	
	OSVERSIONINFO VersionInfo;
	ZeroMemory(&VersionInfo, sizeof(OSVERSIONINFO));
	VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	if (TRUE == GetVersionEx(&VersionInfo))
		m_dwOSMinVersion = VersionInfo.dwMinorVersion;
}

BOOL CPoomContactsSIMReader::InitializeSIM()
{
	HRESULT hr = E_FAIL;
	BOOL bRet = FALSE;

	hr = SimInitialize(0, NULL, 0, &m_hSim);
	if (hr != S_OK)
		return bRet;

	m_simCaps.cbSize = sizeof(SIMCAPS);
	hr = SimGetDevCaps(m_hSim, SIM_CAPSTYPE_ALL, &m_simCaps);
	if (hr != S_OK) {
		SimDeinitialize(m_hSim);
		m_hSim = NULL;
		return bRet;
	}
	
	hr = SimGetPhonebookStatus(m_hSim, SIM_PBSTORAGE_SIM, &m_dwUsed, &m_dwTotal);
	if (hr != S_OK || m_dwUsed == 0) {
		SimDeinitialize(m_hSim);
		m_hSim = NULL;
		return bRet;
	}

	return (bRet = TRUE);
}

CPoomContactsSIMReader::~CPoomContactsSIMReader(void)
{
	if (m_hSim != NULL) {
		SimDeinitialize(m_hSim);
		m_hSim = NULL;
	}
}

INT CPoomContactsSIMReader::Count()
{
	int uCount = 0;
	uCount = m_simCaps.dwMaxPBIndex - m_simCaps.dwMinPBIndex;
	return uCount;
}

HRESULT CPoomContactsSIMReader::Get(INT i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *puBufLen)
{
	HRESULT hr = E_FAIL;
	if (pBuf == NULL || puBufLen == NULL)
		return hr;

	BOOL bGot = FALSE;

	CPoomContact *contact = new CPoomContact();
	if (contact == NULL)
		return hr;
	
	HeaderStruct *header = contact->Header();
	
	if (m_dwOSMinVersion != 1) {		// Se Win mobile e' diverso da 5.0 provo a prendere le info estese
		SIMPHONEBOOKCAPS PBCaps;
		ZeroMemory(&PBCaps, sizeof(SIMPHONEBOOKCAPS));
		PBCaps.cbSize = sizeof(SIMPHONEBOOKCAPS);
		PBCaps.dwParams = SIM_PARAM_PBCAPS_ALL;
		PBCaps.dwStorages = SIM_PBSTORAGE_SIM;
		hr = SimGetPhonebookCapabilities(m_hSim, &PBCaps);
		
		if (hr == S_OK) {
			DWORD dwBufferSize = (sizeof(SIMPHONEBOOKENTRYEX) + PBCaps.dwAdditionalNumberCount*sizeof(SIMPHONEBOOKADDITIONALNUMBER)+
								PBCaps.dwEmailAddressCount*sizeof(SIMPHONEBOOKEMAILADDRESS)); 
			DWORD dwCount = 1;
			dwBufferSize = dwBufferSize * 1;
			LPSIMPHONEBOOKENTRYEX lpSimPBEx = (LPSIMPHONEBOOKENTRYEX)LocalAlloc(LPTR, dwBufferSize);
			hr = SimReadPhonebookEntries(m_hSim, SIM_PBSTORAGE_SIM, i, &dwCount, lpSimPBEx, &dwBufferSize);

			if (hr != S_OK) {
				hr = SimReadPhonebookEntries(m_hSim, SIM_PBSTORAGE_SIM, i, &dwCount, lpSimPBEx, &dwBufferSize);
			}

			header->lOid = lpSimPBEx->dwIndex;
			hr = ParseEx(lpSimPBEx, contact);
			
			LocalFree(lpSimPBEx);

			if (hr == S_OK)
				bGot = TRUE;
		}		
	}

	if (bGot == FALSE) {
		SIMPHONEBOOKENTRY simEntry;
		ZeroMemory(&simEntry, sizeof(SIMPHONEBOOKENTRY));
		simEntry.cbSize = sizeof( SIMPHONEBOOKENTRY );

		hr = SimReadPhonebookEntry(m_hSim,
									SIM_PBSTORAGE_SIM,
									i,
									&simEntry );
				   
		if (hr != S_OK ) {
			if (contact)
				delete contact;
			return hr;
		}

		if (hr == S_OK) {
			header->lOid = i;
			hr = Parse(&simEntry, contact);
			if (hr != S_OK) {
				if (contact)
					delete contact;
				return hr;
			}
		}	
	}

	*pBuf = pPoomSerializer->Serialize(contact, (LPDWORD) puBufLen);

	if(contact)
		delete contact;

	return hr;
}

HRESULT CPoomContactsSIMReader::Parse(SIMPHONEBOOKENTRY* lpsimEntry, CPoomContact *contact)
{
	UINT uAddressLen = 0, uTextLen = 0;
	StringCchLengthW(lpsimEntry->lpszAddress, MAX_LENGTH_ADDRESS, &uAddressLen);	
	StringCchLengthW(lpsimEntry->lpszText, MAX_LENGTH_PHONEBOOKENTRYTEXT, &uTextLen);

	if (uAddressLen == 0 && uTextLen == 0) 
		return S_FALSE;

	ContactMapType* contactMap = contact->Map();

	if (uAddressLen > 0) {
		LPWSTR lpwAddress = new(nothrow) WCHAR[uAddressLen + 1];

		if (lpwAddress != NULL) {
			StringCchCopyW(lpwAddress, uAddressLen+1, lpsimEntry->lpszAddress);
			(*contactMap)[MobileTelephoneNumber] = lpwAddress;
		}
	}

	if (uTextLen > 0) {
		LPWSTR lpwText = new(nothrow) WCHAR[uTextLen + 1];

		if (lpwText != NULL) {
			StringCchCopyW(lpwText, uTextLen+1, lpsimEntry->lpszText);
			(*contactMap)[FirstName] = lpwText;
		}
	}

	return S_OK;
}

HRESULT CPoomContactsSIMReader::ParseEx(SIMPHONEBOOKENTRYEX* lpSimPBEx, CPoomContact *contact)
{
	UINT uAddressLen = 0, uTextLen = 0, uTmpLen = 0;;
	StringCchLengthW(lpSimPBEx->lpszAddress, MAX_LENGTH_ADDRESS, &uAddressLen);	
	StringCchLengthW(lpSimPBEx->lpszText, MAX_LENGTH_PHONEBOOKENTRYTEXT, &uTextLen);

	if (uAddressLen == 0 && uTextLen == 0) {
		return S_FALSE;
	}

	ContactMapType* contactMap = contact->Map();

	if (uAddressLen > 0) {
		LPWSTR lpwAddress = new(nothrow) WCHAR[uAddressLen + 1];

		if (lpwAddress != NULL) {
			StringCchCopyW(lpwAddress, MAX_LENGTH_ADDRESS+1, lpSimPBEx->lpszAddress);
			(*contactMap)[MobileTelephoneNumber] = lpwAddress;
		}
	}

	if (uTextLen > 0) {
		LPWSTR lpwText = new(nothrow) WCHAR[uTextLen + 1];

		if (lpwText != NULL) {
			StringCchCopyW(lpwText, MAX_LENGTH_PHONEBOOKENTRYTEXT+1, lpSimPBEx->lpszText);
			(*contactMap)[FirstName] = lpwText;
		}
	}

	if(SUCCEEDED(StringCchLengthW(lpSimPBEx->lpszSecondName, MAX_LENGTH_PHONEBOOKENTRYTEXT, &uTmpLen))) {
		if (uTmpLen > 0) {
			LPWSTR lpwTemp = new(nothrow) WCHAR[uTmpLen + 1];

			if (lpwTemp != NULL) {
				StringCchCopyW(lpwTemp, uTextLen+1, lpSimPBEx->lpszSecondName);
				(*contactMap)[LastName] = lpwTemp;
			}	
		}
	}


	// Emails
	UINT uMaxNum = 0;
	INT iEmailArray[3] = {Email1Address, Email2Address, Email3Address};
	lpSimPBEx->dwEmailCount < 3? uMaxNum = lpSimPBEx->dwEmailCount: uMaxNum = 3;
	
	for(UINT i = 0; i < uMaxNum; i++) {
		if (lpSimPBEx->lpEmailAddresses[i].lpszAddress == NULL)
			continue;
		
		if(SUCCEEDED(StringCchLengthW(lpSimPBEx->lpEmailAddresses[i].lpszAddress, MAX_LENGTH_PHONEBOOKENTRYTEXT, &uTmpLen))) {
			if (uTmpLen > 0) {
				LPWSTR lpwTemp = new(nothrow) WCHAR[uTmpLen + 1];

				if (lpwTemp != NULL) {
					StringCchCopyW(lpwTemp, uTmpLen+1, lpSimPBEx->lpEmailAddresses[i].lpszAddress);
					(*contactMap)[(e_contactEntry)iEmailArray[i]] = lpwTemp;
				}	
			}
		}
	}

	// numbers
	uMaxNum = 0;
	INT iNumArray[5] = {BusinessTelephoneNumber, HomeTelephoneNumber, HomeFaxNumber, CarTelephoneNumber, RadioTelephoneNumber};
	lpSimPBEx->dwAdditionalNumberCount < 5? uMaxNum = lpSimPBEx->dwEmailCount: uMaxNum = 5;

	for(UINT i = 0; i < uMaxNum; i++) {
		if (lpSimPBEx->lpAdditionalNumbers[i].lpszAddress == NULL)
			continue;

		if(SUCCEEDED(StringCchLengthW(lpSimPBEx->lpAdditionalNumbers[i].lpszAddress, MAX_LENGTH_ADDRESS, &uTmpLen))) {
			if (uTmpLen > 0) {
				LPWSTR lpwTemp = new(nothrow) WCHAR[uTmpLen + 1];

				if (lpwTemp != NULL) {
					StringCchCopyW(lpwTemp, uTextLen+1, lpSimPBEx->lpAdditionalNumbers[i].lpszAddress);
					(*contactMap)[(e_contactEntry)iNumArray[i]] = lpwTemp;
				}	
			}
		}
	}

	return S_OK;
}