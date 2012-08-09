#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#ifdef WINCE
#include <cemapi.h>
#else
#include <mapidefs.h>
#endif
#include <list>
#pragma warning(pop)

#include "MAPIAgentConfiguration.h"
#include "MAPIMessageSerializer.h"
#include "Common.h"

/*
struct rawEntryID {
ULONG cbSize;
LPBYTE lpEntryID;
};
*/

/*
#define PR_BODY_HTML_W 0x1013001F
#define PR_BODY_HTML_A 0x1013001E
#define PR_BODY_HTML PR_BODY_HTML_W
*/

class MAPIAttachment;
struct MAPISerializedMessageHeader;

class MAPIMessage
{
private:
	static const CHAR _filenameLength = 16;

	LPMESSAGE _pMsg;
	LPSPropValue _pMsgProps;
	ULONG _cbMsgProps;
	TCHAR _lpFilename[_filenameLength];
	LPMAPITABLE _pAttachTable;

	LPTSTR _Folder;
	LPTSTR _Class;
	LPTSTR _From;
	LPTSTR _To;

	/*
	LPTSTR _Cc;
	LPTSTR _Bcc;
	*/

	LPTSTR _Subject;
	LPFILETIME _DeliveryTime;
	BOOL _hasAttach;
	DWORD _cbMimeBodySize;
	LPBYTE _lpMimeBody;

	DWORD _cbTextBodySize;
	LPBYTE _lpTextBody;

	MAPISerializedMessageHeader _header;

	HRESULT _GetProperties(void);
	HRESULT _ParseProperties(void);
	HRESULT _GetRecipients(void);
	HRESULT _WriteRecipientsToString(LPTSTR pszTo);
	HRESULT _FormatDateTime( LPFILETIME ft, LPTSTR lpwDate, LPTSTR lpwTime );
	HRESULT _GetMessageBody();

	HRESULT _OpenAttachments(void);
	LPBYTE _GetAttachmentBody( LPATTACH pAttach, LPDWORD dwSize );

	DWORD _SerializedStringLength(LPWSTR lpString)
	{
		DWORD cbStringLength = 0;
		cbStringLength += sizeof(DWORD);

		if (lpString == NULL)
			return cbStringLength;

		cbStringLength += (wcslen(lpString) + 1) * sizeof(TCHAR);

		return cbStringLength;
	}  

public:
	std::list<MAPIAttachment*> attachments;

	// MAPIMessage() {} // to be used ONLY when unserializing object!!!
	MAPIMessage(IMessage *pMsg);
	virtual ~MAPIMessage(void);

	// setters/getters
	void SetFolder(LPTSTR lpFolder) { _Folder = lpFolder; }
	LPTSTR Folder() { return _Folder; }

	void SetDeliveryTime( LPFILETIME ft ) { _DeliveryTime = ft; }
	LPFILETIME DeliveryTime() { return _DeliveryTime; }

	void SetClass( LPTSTR lpClass ) { _Class = lpClass; }
	LPTSTR Class() { return _Class; }

	void SetFrom( LPTSTR lpFrom ) { _From = lpFrom; } 
	LPTSTR From() { return _From; }

	void SetTo( LPTSTR lpTo ) { _To = lpTo; }
	LPTSTR To() { return _To; }

	/*
	void SetCC( LPTSTR lpCc ) { _Cc = lpCc; }
	LPTSTR Cc() { return _Cc; }

	void SetBcc( LPTSTR lpBcc ) { _Bcc = lpBcc; }
	LPTSTR Bcc() { return _Bcc; }
	*/

	LONG Size();

	void SetSubject( LPTSTR lpSubject ) { _Subject = lpSubject; }
	LPTSTR Subject() { return _Subject; }

	void setIncoming() { _header.Flags |= MESSAGE_INCOMING; }

	void SetMIMEBody( LPBYTE lpBody, DWORD cbSize ) { _lpMimeBody = lpBody; _cbMimeBodySize = cbSize; }
	LPBYTE MIMEBody(LPDWORD lpdwSize) { (*lpdwSize) = _cbMimeBodySize; return _lpMimeBody; } 

	void SetTEXTBody( LPBYTE lpBody, DWORD cbSize) { _lpTextBody = lpBody; _cbTextBodySize = cbSize; }
	LPBYTE TEXTBody(LPDWORD lpdwSize) { (*lpdwSize) = _cbTextBodySize; return _lpTextBody; }

	BOOL hasBody() 
	{
		if (_cbTextBodySize > 0 || _cbMimeBodySize > 0)
			return TRUE;
		return FALSE;
	}

	BOOL hasSubject() {
		return (_Subject != NULL ? TRUE : FALSE);
	}
	
	BOOL isMail() {
		return CmpWildW(CLASS_MAIL, this->Class());
	}
	
	BOOL isSMS() {
		return CmpWildW(CLASS_SMS, this->Class());
	}

	BOOL isMMS() {
		return CmpWildW(CLASS_MMS, this->Class());
	}
	
	LPMESSAGE GetIMessagePtr(void) { return _pMsg; }
	MAPISerializedMessageHeader* Header() { return &_header; }

	HRESULT Print(void);
	HRESULT Parse(FilterTypes type);

	// ATTACHMENTS
	HRESULT GetAttachments(void);
	void SetNumberOfAttach(DWORD nAttachs);
	BOOL HasAttachments() { return _hasAttach; }
	MAPIAttachment* GetSingleAttachment(void);

	// SERIALIZATION
	LPBYTE Serialize(LPDWORD lpdwLength);
};
