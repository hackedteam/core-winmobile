#include "MAPI.h"
#include "MAPIStorage.h"

MAPI::MAPI(void) 
: _pSession(NULL), _pTable(NULL), _storesEnumerated(false)
{
}

MAPI::~MAPI(void)
{
  std::list<MAPIStorage*>::iterator i = stores.begin();
  while (i != stores.end())
  {
    delete *i;
    stores.erase(i++);
  }
  
  if (_pTable)
    _pTable->Release();

  if (_pSession)
  {
    _pSession->Logoff(0, 0, 0);
    _pSession->Release();
  }

  MAPIUninitialize();
}

HRESULT MAPI::EnumerateStores(void)
{
	HRESULT hr;
  
	static const SizedSPropTagArray (2, spta) = {2, PR_DISPLAY_NAME, PR_ENTRYID};
	hr = _pTable->SetColumns((SPropTagArray *) &spta, 0);
  
	if (FAILED(hr))
		return E_FAIL;
  
	LOOP {
		LPSRowSet pRowSet = NULL;
		
		hr = _pTable->QueryRows(1, 0, &pRowSet);
		if (hr == MAPI_E_BUSY)
		{
			Sleep(10);
			continue;
		}
		else if ((hr != S_OK) || (pRowSet == NULL) || (pRowSet->cRows != 1))
		{
			FreeProws(pRowSet);
			break;
		}
		
		ASSERT( pRowSet->aRow[0].lpProps[0].ulPropTag == PR_DISPLAY_NAME);
		ASSERT( pRowSet->aRow[0].lpProps[1].ulPropTag == PR_ENTRYID);
    
		IMsgStore * pStore;
		
		hr = _pSession->OpenMsgStore(0,
									pRowSet->aRow[0].lpProps[1].Value.bin.cb,	//ulStoreBytes,
									(LPENTRYID) pRowSet->aRow[0].lpProps[1].Value.bin.lpb,	// pEntry
									0,
									0,
									&pStore);
    if (FAILED(hr)) {
      FreeProws(pRowSet);
      continue;
    }

    MAPIStorage * storage = new MAPIStorage(pRowSet->aRow[0].lpProps[0].Value.lpszW, pStore);
    stores.push_front(storage);
	  
		FreeProws(pRowSet);
  }
  
  _storesEnumerated = true;
  
	return S_OK;
}

MAPIStorage* MAPI::GetStoreByName( WCHAR *name )
{
  if (!name)
    return NULL;
  
  if (!_storesEnumerated)
    this->EnumerateStores();

  for(std::list<MAPIStorage*>::iterator store_iter = stores.begin(); 
    store_iter != stores.end(); store_iter++)
  {
    if (_tcscmp((*store_iter)->name(), name))
      return *store_iter;
  }

  return NULL;
}

HRESULT MAPI::Init(void)
{
	HRESULT hr;
	
	hr = MAPIInitialize(NULL);
	if (hr != S_OK)
		return hr;
	
	hr = MAPILogonEx( 0, NULL, NULL, 0, (LPMAPISESSION *) &_pSession);
	if (hr != S_OK)	
		return hr;

	hr = _pSession->GetMsgStoresTable(MAPI_UNICODE, &_pTable);
	if (hr != S_OK)	
		return hr;

	return S_OK;
}
