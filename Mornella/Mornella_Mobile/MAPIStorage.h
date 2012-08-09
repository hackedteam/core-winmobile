#pragma once

#include "MAPICommon.h"
#include "MAPIFolder.h"
#pragma warning(push, 3)
#include "tree.hh"
#pragma warning(pop)

class MAPIStorage
{
private:
	LPWSTR _name;
	IMsgStore * _store;

  bool _enumeratedFolders;
  
  tree<MAPIFolder*>::iterator _GetRootFolder(void);
  HRESULT _EnumerateSubFolders( tree<MAPIFolder*>::iterator head );
  
  rawEntryID _incomingFolder;
  
public:
  tree<MAPIFolder*> folders;
  
	MAPIStorage(LPWSTR name, IMsgStore * store);
	virtual ~MAPIStorage(void);
  
  IMsgStore* GetIMsgStorePtr() { return _store; }

  bool isIncomingFolder(MAPIFolder* folder)
  {
	  if (memcmp(folder->entryID.lpEntryID, _incomingFolder.lpEntryID, folder->entryID.cbSize))
		  return true;
	  return false;
  }
  
  LPWSTR name(void);
  HRESULT BuildFoldersTree(void);
  LPWSTR FolderPath( tree<MAPIFolder*>::iterator folder_iter );
  HRESULT GetSentMailFolder();
};
