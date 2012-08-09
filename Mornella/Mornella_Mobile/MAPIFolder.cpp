#include "MAPIMessage.h"
#include "MAPIFolder.h"

#include <new>

#define INITGUID
#define USES_IID_IMAPIProp
#include <initguid.h>
#include <mapiguid.h>

MAPIFolder::MAPIFolder(LPWSTR name, IMAPIFolder * folder)
: _pFolder(folder), _pContentsTable(NULL), _name(NULL)
{
	ASSERT(_pFolder);
  
  size_t len = _tcslen(name) + 1;
  _name = new(std::nothrow) WCHAR[len];
  if (_name != NULL) {
    memset(_name, 0, len * sizeof(WCHAR));
    wcscpy_s(_name, len, name);
  }
}

MAPIFolder::~MAPIFolder(void)
{
  if (entryID.lpEntryID)
	  delete [] entryID.lpEntryID;

  if (_pFolder)
	  _pFolder->Release();

  if (_pContentsTable)
    _pContentsTable->Release();

  if (_name)
    delete [] _name;
}

HRESULT MAPIFolder::Open()
{
  static const SizedSSortOrderSet(1, sortOrderSet) = { 1, 0, 0, { PR_MESSAGE_DELIVERY_TIME, TABLE_SORT_DESCEND } };
  
  HRESULT hr = E_FAIL;
  
  if (_pFolder ==  NULL)
    return E_FAIL;
  
  // Open the folder's contents table
  hr = _pFolder->GetContentsTable (MAPI_UNICODE, &_pContentsTable);
  if (FAILED(hr))
    return E_FAIL;
  
  // Sort the table that we obtained by time
  hr = _pContentsTable->SortTable((SSortOrderSet *)&sortOrderSet, 0);
  if (FAILED(hr))
  {
    if (_pContentsTable)
      _pContentsTable->Release();
    return E_FAIL;
  }
  
  return S_OK;
}

MAPIMessage* MAPIFolder::GetSingleMessage()
{ 
  ASSERT(_pContentsTable);
  ASSERT(_pFolder);

  // Get a row
  LPSRowSet pRowSet = NULL; 
  HRESULT hr = _pContentsTable->QueryRows (1, 0, &pRowSet);
  if (FAILED(hr))  
    return NULL;
  
  // Did we hit the end of the table?
  if (pRowSet->cRows != 1)
    return NULL;
  
  // Get message entry ID
  ULONG cbEntryID = pRowSet[0].aRow[0].lpProps[0].Value.bin.cb;
  LPENTRYID lpEntryID = reinterpret_cast<ENTRYID*>(pRowSet[0].aRow[0].lpProps[0].Value.bin.lpb);
  
  // Open this message
  LPMESSAGE pMsg = NULL;
  hr = _pFolder->OpenEntry (cbEntryID, lpEntryID, NULL, 0, NULL, reinterpret_cast<IUnknown **>(&pMsg));
  if (FAILED(hr)) 
  {
    FreeProws(pRowSet);
    return NULL;
  }

  FreeProws (pRowSet);          
  pRowSet = NULL;

  MAPIMessage* msg = new(std::nothrow) MAPIMessage(pMsg);
  return msg;
}

LONG MAPIFolder::GetNumberOfMessages()
{
	HRESULT hr = E_FAIL;
	
	ULONG rgTags[] = {2, PR_CONTENT_COUNT, PR_FOLDER_TYPE};
	ULONG cValues = 0;
	
	IMAPIProp * pProp = NULL; 
	SPropValue * rgFolderProps = NULL;
	
	hr = _pFolder->QueryInterface(IID_IMAPIProp, (LPVOID *) &pProp);
	if (FAILED(hr))
	{
		return -1;
	}
	if (!pProp)
	{
		return -1;
	}
	
	hr = pProp->GetProps((LPSPropTagArray)rgTags, MAPI_UNICODE, &cValues, &rgFolderProps);
	if (FAILED(hr))
	{
		if (pProp)
      pProp->Release();
		return -1;
	}
	if (PR_CONTENT_COUNT != rgFolderProps[0].ulPropTag)
	{
		if (pProp)
      pProp->Release();
		return -1;
	}
	if (PR_FOLDER_TYPE != rgFolderProps[1].ulPropTag)
	{
		if (pProp)
      pProp->Release();
		return -1;
	}

  ULONG ulNumberOfMessages = rgFolderProps[0].Value.ul;

  pProp->Release();
  MAPIFreeBuffer(rgFolderProps);
	
	return ulNumberOfMessages;
}

LPWSTR MAPIFolder::name( void )
{
  return this->_name;
}

IMAPIFolder * MAPIFolder::ptrToIMAPIFolder( void )
{
  return this->_pFolder;
}
