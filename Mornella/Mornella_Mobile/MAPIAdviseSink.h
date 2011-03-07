#pragma once

#pragma warning(push, 3)
#include <ObjBase.h>
#include <Windows.h>
#include <cemapi.h>
#include <mapiutil.h>
#include <list>
#pragma warning(pop)

#include "MAPICommon.h"

class MAPIMessage;

class CMAPIAdviseSink :
	public IMAPIAdviseSink
{
	long m_lRef;
  IMsgStore* m_pStore;
  ULONG m_ulConnectionNumber;
  IMAPISession& _session;
  
  std::list<rawEntryID*> _ignoredFolders;
  
  bool _GetIgnoredFolders();
  bool _IsIgnoredFolder(ULONG cbEntryId, LPBYTE lpEntryID);
  
public:
  
	CMAPIAdviseSink(IMAPISession& session);
	~CMAPIAdviseSink(void);
	
    void SetStore(IMsgStore* pMsgStore) { m_pStore = pMsgStore; _GetIgnoredFolders(); }
	void SetConnectionNumber(ULONG connNumber) { m_ulConnectionNumber = connNumber; }
	
	void Unadvise();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMAPIAdviseSink
	STDMETHODIMP_(ULONG) OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifications);

	BOOL LogMessage(MAPIMessage& message);
	
};
