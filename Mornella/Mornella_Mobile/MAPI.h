#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#include <cemapi.h>
#include <mapiutil.h>
#pragma warning(pop)

#include <list>

#include "MAPICommon.h"
#include "MAPIStorage.h"

class MAPI
{
private:
	ICEMAPISession * _pSession;
	IMAPITable * _pTable;
  bool _storesEnumerated;

public:
	MAPI(void);
	~MAPI(void);
  
  std::list<MAPIStorage*> stores;

	HRESULT EnumerateStores(void);
  MAPIStorage* GetStoreByName(WCHAR *name);
  
	HRESULT Init(void);
};
