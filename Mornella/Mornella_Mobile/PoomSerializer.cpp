#include <pimstore.h>

#include "PoomSerializer.h"
#include "PoomCommon.h"

#include "PoomContact.h"
#include "PoomCalendar.h"
#include "PoomTask.h"

#include "Common.h"

LPBYTE CPoomSerializer::Serialize(CPoomCalendar *calendar, LPDWORD lpdwOutLength) {
	DWORD dwDynLen = 0;
	LPBYTE pPtr = NULL;
	LPBYTE lpOutBuf = NULL;
	LONG lTmp = 0;

	if (lpdwOutLength == NULL) {
		DBG_TRACE(L"PoomSerializer.cpp - Serialize lpdwOutLength == NULL ", 5, FALSE);
		return lpOutBuf;	
	}

	// Calculate buffer total length
	*lpdwOutLength = 0;

	// Header length
	HeaderStruct* header = NULL;
	header = calendar->Header();
	header->dwVersion = POOM_V1_0_PROTO;
	*lpdwOutLength = sizeof(HeaderStruct);
	
	// FLAGS + StartDate + EndDate + 5 Long
	*lpdwOutLength += sizeof(DWORD) + 2*sizeof(FILETIME) + 4*sizeof(LONG);

	// Strings length
	*lpdwOutLength += _SerializedStringLength(calendar->Subject());
	*lpdwOutLength += _SerializedStringLength(calendar->Categories());
	*lpdwOutLength += _SerializedStringLength(calendar->Body());
	*lpdwOutLength += _SerializedStringLength(calendar->Location());
	*lpdwOutLength += _SerializedStringLength(calendar->Recipients());
	
	// Recurrence struct (if necessary)
	if ((calendar->Flags() & FLAG_RECUR) && calendar->GetRecurStruct() != NULL)
		*lpdwOutLength += sizeof(RecurStruct);

	header->dwSize = *lpdwOutLength;

	// Fill buffer
	lpOutBuf = new(std::nothrow) BYTE[*lpdwOutLength];

	if (lpOutBuf == NULL)
		return NULL;

	pPtr = lpOutBuf;

	// Header
	CopyMemory( pPtr, header, sizeof(HeaderStruct));
	pPtr += sizeof(HeaderStruct);

	// Flags
	lTmp = calendar->Flags();
	CopyMemory(pPtr, &lTmp, sizeof(DWORD));
	pPtr += sizeof(DWORD);

	// StartDate + EndDate
	CopyMemory(pPtr, &calendar->StartDate(), sizeof(FILETIME));
	pPtr += sizeof(FILETIME);

	CopyMemory(pPtr, &calendar->EndDate(), sizeof(FILETIME));
	pPtr += sizeof(FILETIME);

	// 5xLong
	SETLONG(calendar->Sensitivity);
	SETLONG(calendar->BusyStatus);
	SETLONG(calendar->Duration);
	SETLONG(calendar->MeetingStatus);
	
	// Recurrence (if encessary) 
	if ((calendar->Flags() & FLAG_RECUR) && calendar->GetRecurStruct()) {
		memcpy_s(pPtr, sizeof(RecurStruct), calendar->GetRecurStruct(), sizeof(RecurStruct));
		pPtr += sizeof(RecurStruct);
	}
	
	// Strings
	if (calendar->Subject()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, calendar->Subject(), POOM_STRING_SUBJECT);
		pPtr += dwSerializedStringSize;
	}

	if (calendar->Categories()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, calendar->Categories(), POOM_STRING_CATEGORIES);
		pPtr += dwSerializedStringSize;
	}

	if (calendar->Body()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, calendar->Body(), POOM_STRING_BODY);
		pPtr += dwSerializedStringSize;
	}

	if (calendar->Recipients()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, calendar->Recipients(), POOM_STRING_RECIPIENTS);
		pPtr += dwSerializedStringSize;
	}

	if (calendar->Location()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, calendar->Location(), POOM_STRING_LOCATION);
		pPtr += dwSerializedStringSize;
	}

	return lpOutBuf;
}

LPBYTE CPoomSerializer::Serialize(CPoomContact *contact, LPDWORD lpdwOutLength) {
	
	DWORD dwDynLen = 0;
	LPBYTE pPtr = NULL, lpOutBuf = NULL;
	ContactMapType* pMap = NULL;
	HeaderStruct* header = NULL;

	pMap = contact->Map();
	
	header = contact->Header();
	header->dwVersion = POOM_V1_0_PROTO;
	*lpdwOutLength = sizeof(HeaderStruct);

	// Obtain dynamic entries length after serialization
	for (ContactMapType::iterator it = pMap->begin(); it != pMap->end(); it++)
		*lpdwOutLength += _SerializedStringLength(it->second);
	
	header->dwSize = *lpdwOutLength;

	lpOutBuf = new(std::nothrow) BYTE[*lpdwOutLength];

	if (lpOutBuf == NULL)
		return NULL;

	pPtr = lpOutBuf;

	// Copy header
	CopyMemory( pPtr, header, sizeof(HeaderStruct));
	pPtr += sizeof(HeaderStruct);

	// Serialize and copy strings
	for (ContactMapType::iterator it =  pMap->begin(); it != pMap->end(); it++){
		DWORD dwSerializedStringSize = _SerializeString(pPtr, it->second, (it->first) << 0x18);
		pPtr += dwSerializedStringSize;
	}

	return lpOutBuf;
}

LPBYTE CPoomSerializer::Serialize(CPoomTask *task, LPDWORD lpdwOutLength) {
	DWORD dwDynLen = 0;
	LPBYTE pPtr = NULL;
	LPBYTE lpOutBuf = NULL;
	LONG lTmp = 0;

	// Calculate buffer total length
	*lpdwOutLength = 0;

	// Header length
	HeaderStruct* header = NULL;
	header = task->Header();
	header->dwVersion = POOM_V1_0_PROTO;
	*lpdwOutLength = sizeof(HeaderStruct);

	// FLAGS + 3 Date +  + 2 Long
	*lpdwOutLength += sizeof(DWORD) + 3 * sizeof(FILETIME) + 2 * sizeof(LONG);

	// Strings length
	*lpdwOutLength += _SerializedStringLength(task->Subject());
	*lpdwOutLength += _SerializedStringLength(task->Categories());
	*lpdwOutLength += _SerializedStringLength(task->Body());
	
	// Recurrence struct (if necessary)
	if ((task->Flags() & FLAG_RECUR))
		*lpdwOutLength += sizeof(RecurStruct);

	header->dwSize = *lpdwOutLength;

	// Fill buffer
	lpOutBuf = new(std::nothrow) BYTE[*lpdwOutLength];

	if (lpOutBuf == NULL)
		return NULL;

	pPtr = lpOutBuf;

	// Header
	CopyMemory( pPtr, header, sizeof(HeaderStruct));
	pPtr += sizeof(HeaderStruct);

	// Flags
	lTmp = task->Flags();
	CopyMemory(pPtr, &lTmp, sizeof(DWORD));
	pPtr += sizeof(DWORD);
	
	// DATE
	CopyMemory(pPtr, &task->DueDate(), sizeof(FILETIME));
	pPtr += sizeof(FILETIME);

	CopyMemory(pPtr, &task->StartDate(), sizeof(FILETIME));
	pPtr += sizeof(FILETIME);

	CopyMemory(pPtr, &task->DateCompleted(), sizeof(FILETIME));
	pPtr += sizeof(FILETIME);

	SETLONG(task->Sensitivity);
	SETLONG(task->Importance);

	// Recurrence (if necessary) 
	if ((task->Flags() & FLAG_RECUR)) {
		CopyMemory(pPtr, task->GetRecurStruct(), sizeof(RecurStruct));
		pPtr += sizeof(RecurStruct);
	}

	// Strings
	if (task->Subject()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, task->Subject(), POOM_STRING_SUBJECT);
		pPtr += dwSerializedStringSize;
	}

	if (task->Categories()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, task->Categories(), POOM_STRING_CATEGORIES);
		pPtr += dwSerializedStringSize;
	}

	if (task->Body()) {
		DWORD dwSerializedStringSize = _SerializeString(pPtr, task->Body(), POOM_STRING_BODY);
		pPtr += dwSerializedStringSize;
	}

	return lpOutBuf;
}

DWORD CPoomSerializer::_SerializedStringLength(LPWSTR lpString)
{
	DWORD dwStringLength = 0;

	if (lpString == NULL || wcslen(lpString) == 0)
		return 0;
	
	dwStringLength = sizeof(DWORD);
	dwStringLength += wcslen(lpString)*sizeof(WCHAR);
	
	return dwStringLength;
}

DWORD CPoomSerializer::_SerializeString(LPBYTE lpDest, LPTSTR lpString, int entryType)
{
	DWORD dwStringLength = 0;

	// if string is invalid, ignore and return 0 as length
	if (lpString == NULL)
		return 0;

	// if string length == 0, ignore and return 0 as length
	dwStringLength = wcslen(lpString) * sizeof(WCHAR); 

	if (dwStringLength == 0)
		return 0;

	// copy prefix and string *** WITHOUT NULL TERMINATOR ***
	DWORD prefix = _Prefix(dwStringLength, entryType);
	CopyMemory(lpDest, &prefix, sizeof(prefix));
	CopyMemory((lpDest + sizeof(prefix)), lpString, dwStringLength);

	return sizeof(prefix) + dwStringLength;
}

DWORD CPoomSerializer::_Prefix(DWORD dwLength, int entryType)
{
	DWORD prefix = dwLength;
	prefix &= POOM_TYPE_MASK;    // clear type bits
	prefix |= (DWORD)entryType; 

	return prefix;
}