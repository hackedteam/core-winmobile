#include "MAPIStorageAdviser.h"
#include "MAPIAdviseSink.h"

// #include <MAPIUtil.h>

MAPIStorageAdviser::MAPIStorageAdviser(void)
{
}

MAPIStorageAdviser::~MAPIStorageAdviser(void)
{
	std::list<CMAPIAdviseSink*>::iterator iter = _sinks.begin();
	while (iter != _sinks.end())
	{
		(*iter)->Unadvise();
		delete *iter;
		_sinks.erase(iter++);
	}
}

IMAPISession * MAPIStorageAdviser::InitIMAPISession()
{
  if(FAILED(MAPIInitialize(NULL)))
    return NULL;
  
  if(MAPILogonEx(0, NULL, NULL, 0, &_pIMapiSession) != S_OK) {
    MAPIUninitialize();
    return NULL;
  }
  
  return _pIMapiSession;
}

IMAPITable * MAPIStorageAdviser::GetStoresTable( IMAPISession* pSession )
{
  ASSERT(pSession);

  IMAPITable * pTable = NULL;
  HRESULT hr = pSession->GetMsgStoresTable(0, &pTable);
  if(FAILED(hr)){
    return NULL;
  }

  return pTable;
}

IMAPIAdviseSink * MAPIStorageAdviser::_GetSink( CMAPIAdviseSink * pMapiNotifySink )
{
  ASSERT(pMapiNotifySink);

  IMAPIAdviseSink * pMapiSink = NULL;
  HRESULT hr = pMapiNotifySink->QueryInterface(IID_IMAPIAdviseSink, (void **)&pMapiSink);
  if(FAILED(hr)){
    pMapiSink->Release();
    return NULL;
  }

  return pMapiSink;
}

bool MAPIStorageAdviser::AddSink( IMsgStore* pStore )
{
  CMAPIAdviseSink * pMapiNotifySink = new CMAPIAdviseSink(*_pIMapiSession);
  IMAPIAdviseSink* pMapiSink = _GetSink(pMapiNotifySink);
  
  ULONG ulConnNum = 0;
  ULONG ulMask = fnevObjectCreated | fnevObjectMoved; // | fnevObjectModified; // | fnevObjectCopied;
  HRESULT hr = pStore->Advise( 0, NULL, ulMask, pMapiSink, &ulConnNum );
  if (FAILED(hr))
    return false;
  else {
    pMapiNotifySink->SetStore( pStore );
	pMapiNotifySink->SetConnectionNumber(ulConnNum);
  }
  
  _sinks.push_back(pMapiNotifySink);
  
  return true;
}

bool MAPIStorageAdviser::AdviseAll()
{
  IMAPISession* pSession = InitIMAPISession();
  IMAPITable* pStoresTable = GetStoresTable(pSession); 
  
  SizedSPropTagArray(2, tblColumns) = {2, {PR_DISPLAY_NAME, PR_ENTRYID}};
  pStoresTable->SetColumns((LPSPropTagArray)&tblColumns, 0);
  
  for(;;) {
    SRowSet * pRowSet = NULL;

    pStoresTable->QueryRows(1, 0, &pRowSet);
    
    if(pRowSet->cRows != 1)
      break;

    ASSERT(pRowSet->aRow[0].cValues == tblColumns.cValues);
    SPropValue* pVal = pRowSet->aRow[0].lpProps;

    ASSERT(pVal[0].ulPropTag == PR_DISPLAY_NAME);
    ASSERT(pVal[1].ulPropTag == PR_ENTRYID);

    LPWSTR lpStoreName = pRowSet->aRow[0].lpProps[0].Value.lpszW;

    // Open Store
    LPBYTE pEntry = (LPBYTE)pRowSet->aRow[0].lpProps[1].Value.bin.lpb;
    ULONG ulStoreBytes = pRowSet->aRow[0].lpProps[1].Value.bin.cb;

    IMsgStore* pStore = _GetStoreFromEntryID(pSession, ulStoreBytes, pEntry);
    if (pStore == NULL)
    {
      continue;
    }
    
	if (AddSink(pStore) == true) {
	}else{
	}

    pStore->Release();
    
    FreeProws(pRowSet);
  }
  
  pStoresTable->Release();
  pSession->Release();
  
  return true;
}

IMsgStore* MAPIStorageAdviser::_GetStoreFromEntryID(IMAPISession* pSession, ULONG ulStoreBytes, LPBYTE pEntry)
{
  IMsgStore* pStore = NULL;
  HRESULT hr = pSession->OpenMsgStore(NULL, ulStoreBytes, (LPENTRYID)pEntry, NULL, NULL, &pStore);
  if (FAILED(hr))
    return NULL;
  
  return pStore;
}
