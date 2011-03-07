#pragma once

#include <list>
#include "MAPICommon.h"
#include "MAPIMessage.h"

class MAPIFolder
{
private:
	LPWSTR _name;
	LPMAPIFOLDER _pFolder;
  LPMAPITABLE _pContentsTable;
  
public:
  MAPIFolder(LPWSTR lpwName, IMAPIFolder * pFolder);
  ~MAPIFolder(void);
  
  rawEntryID entryID;
  std::list<MAPIMessage *> messages;
  
  LPWSTR name(void);
	IMAPIFolder *ptrToIMAPIFolder(void);
  
  LONG GetNumberOfMessages(void);
  HRESULT Open();
  MAPIMessage* GetSingleMessage();
	
  void setEntryID(LPENTRYID id, DWORD size)
  {
	  entryID.lpEntryID = new BYTE[size];
	  memcpy(entryID.lpEntryID, id, size);
	  entryID.cbSize = size;
  }
};
