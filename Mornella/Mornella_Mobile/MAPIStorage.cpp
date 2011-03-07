#pragma warning(push, 3)
#include <windows.h>
#pragma warning(pop)

#include <new>

#include "MAPICommon.h"
#include "MAPIStorage.h"
#include "MAPIFolder.h"
#include "tree.hh"


MAPIStorage::MAPIStorage(LPWSTR name , IMsgStore * store)
: _store(store), _enumeratedFolders(false)
{
  size_t len = _tcslen(name) + 1;
  _name = new(std::nothrow) WCHAR[len];
  if (_name != NULL) {
    memset(_name, 0, len * sizeof(WCHAR));
    wcscpy_s(_name, len, name);
  }
}

MAPIStorage::~MAPIStorage(void)
{
	if (_store)
    _store->Release();
  
  tree<MAPIFolder*>::post_order_iterator iter;
  for (iter = folders.begin_post(); iter != folders.end(); iter++)
  {
    MAPIFolder *folder = *iter;
    delete folder;
    folders.erase(iter);
  }
  
  delete[] _name;
}

tree<MAPIFolder*>::iterator MAPIStorage::_GetRootFolder(void)
{
	HRESULT hr;
  IMAPIFolder* pRootFolder;
  
	static const SizedSPropTagArray(2, rgStoreTags) = {2, {PR_IPM_SUBTREE_ENTRYID, PR_OBJECT_TYPE}};
	
	ULONG ulStoreValues = 0, ulTypeOpenedObj = 0;
	LPSPropValue lpStoreProp = NULL;
	
	hr = _store->GetProps(	(LPSPropTagArray) &rgStoreTags,
									MAPI_UNICODE,
									&ulStoreValues,
									&lpStoreProp);
  if (FAILED(hr)) {
    return folders.end();
  }
	
	// Get Root Folder
	hr = _store->OpenEntry(	lpStoreProp[0].Value.bin.cb,
							(LPENTRYID) lpStoreProp->Value.bin.lpb,
							NULL,
							MAPI_ACCESS_READ,	//MAPI_BEST_ACCESS,
							&ulTypeOpenedObj,
							(LPUNKNOWN*) &pRootFolder);
  if (FAILED(hr)) {
    return folders.end();
  }
	
	MAPIFreeBuffer(lpStoreProp);
  
  tree<MAPIFolder*>::iterator top, root;
  top = folders.begin();
  MAPIFolder* root_folder = new MAPIFolder(L"ROOT", pRootFolder);
  root_folder->setEntryID((LPENTRYID)lpStoreProp->Value.bin.lpb, lpStoreProp[0].Value.bin.cb);
  root = folders.insert(top, root_folder);
	
  return root;
}

LPWSTR MAPIStorage::name( void )
{
  return this->_name;
}

HRESULT MAPIStorage::_EnumerateSubFolders( tree<MAPIFolder*>::iterator head )
{
  HRESULT hr;
  IMAPITable * pFoldersTable = NULL;
  
  IMAPIFolder * pRootFolder = (*head)->ptrToIMAPIFolder();

  hr = pRootFolder->GetHierarchyTable(0, &pFoldersTable);
  if (hr == MAPI_E_NO_SUPPORT) 
  {
    // no child containers
    return S_OK;
  }
  
  SizedSPropTagArray(2, tblColumns) = {2, {PR_DISPLAY_NAME, PR_ENTRYID}};	
  hr = pFoldersTable->SetColumns((LPSPropTagArray)& tblColumns, 0);

  LOOP
  {
    SRowSet *pFolderRowSet = NULL;

    hr = pFoldersTable->QueryRows(1, 0, &pFolderRowSet);

    if(pFolderRowSet->cRows != 1)
    {
      FreeProws(pFolderRowSet);
      break;
    }
    
    IMAPIFolder * pChildFolder = NULL;
    
    ULONG uTypeOfOpenedObject = 0;
    hr = pRootFolder->OpenEntry(pFolderRowSet->aRow[0].lpProps[1].Value.bin.cb,
      (LPENTRYID)pFolderRowSet->aRow[0].lpProps[1].Value.bin.lpb,
      NULL,
      0,
      &uTypeOfOpenedObject,
      (LPUNKNOWN*) &pChildFolder
      );

    if (uTypeOfOpenedObject == MAPI_FOLDER)
    {
      tree<MAPIFolder*>::iterator child;

      LPWSTR lpwFolderName = pFolderRowSet->aRow[0].lpProps[0].Value.lpszW;
      ASSERT(lpwFolderName);

      MAPIFolder* folder = new MAPIFolder(lpwFolderName, pChildFolder);
	  folder->setEntryID((LPENTRYID)pFolderRowSet->aRow[0].lpProps[1].Value.bin.lpb, 
		  pFolderRowSet->aRow[0].lpProps[1].Value.bin.cb);
      child = folders.append_child(head, folder);
      _EnumerateSubFolders(child);
    } else {
      if (pChildFolder)
        pChildFolder->Release();
    }
    
    FreeProws(pFolderRowSet);
    Sleep(10);

  } /* while */

  if (pFoldersTable)
    pFoldersTable->Release();

  return S_OK;
}

HRESULT MAPIStorage::BuildFoldersTree( void )
{
  GetSentMailFolder();

  tree<MAPIFolder*>::iterator root = _GetRootFolder();

  // if no valid root folder is available, fail
  if (root == folders.end())
    return E_FAIL;
  
  _EnumerateSubFolders(root);
  
  return S_OK;
}

LPWSTR MAPIStorage::FolderPath( tree<MAPIFolder*>::iterator folder_iter )
{
  std::list<LPWSTR> DirectoryNames;
  
  tree<MAPIFolder*>::iterator parent_iter = folder_iter;
  while ((parent_iter = folders.parent(parent_iter)) != folders.end()) {
    DirectoryNames.push_front((*parent_iter)->name());
  }
  
  ULONG PathLength = 1;
  for (std::list<LPWSTR>::iterator i = DirectoryNames.begin(); i != DirectoryNames.end(); i++)
  {
    PathLength += wcslen(*i) * sizeof(WCHAR);
    PathLength += 1;
  }
  
  LPWSTR Path = (LPWSTR) LocalAlloc(LPTR, PathLength);
  if (Path == NULL)
    return NULL;
  
  while (!DirectoryNames.empty())
  {
    std::list<LPWSTR>::iterator i = DirectoryNames.begin();
    WCHAR lpwLocalPath[256] = {0};
    wsprintf(lpwLocalPath, _T("\\%s"), *i);
    StringCchCat(Path, PathLength, lpwLocalPath);
    DirectoryNames.pop_front();
  }
  
  DirectoryNames.clear();
  
  return Path;
}

HRESULT MAPIStorage::GetSentMailFolder()
{
	LPSPropValue rgprops = NULL;
	ULONG cValues = 0;
	
	SizedSPropTagArray(1, rgFolderTags) = {1, { PR_IPM_SENTMAIL_ENTRYID } };
	
	HRESULT hr = _store->GetProps((LPSPropTagArray)&rgFolderTags, MAPI_UNICODE, &cValues, &rgprops);
	if(FAILED(hr))
		return E_FAIL;
	
	for (ULONG i = 0; i < 1; i++)
	{
		ULONG cbEntryID = rgprops[i].Value.bin.cb;
		LPENTRYID lpEntryID = (LPENTRYID)rgprops[i].Value.bin.lpb;
		
		_incomingFolder.cbSize = cbEntryID;
		_incomingFolder.lpEntryID = new BYTE[_incomingFolder.cbSize];
		memcpy(_incomingFolder.lpEntryID, (LPBYTE)lpEntryID, _incomingFolder.cbSize);
		
		IMAPIFolder* pFolder = NULL;
		hr = _store->OpenEntry(
			_incomingFolder.cbSize,
			(LPENTRYID)_incomingFolder.lpEntryID,
			NULL, 
			MAPI_MODIFY,
			NULL, 
			(LPUNKNOWN*)&pFolder);
		if (!pFolder)
			return E_FAIL;
		
		SizedSPropTagArray(1, ptaFolder) = {1, {PR_DISPLAY_NAME}};
		LPSPropValue rgFolderProps = NULL;
		pFolder->GetProps((LPSPropTagArray)&ptaFolder, MAPI_UNICODE, &cValues, &rgFolderProps);
		LPWSTR lpFolderName = rgFolderProps[0].Value.lpszW;
	}

	return S_OK;
}
