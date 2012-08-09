#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#include <list>
#pragma warning(pop)

class CMAPIAdviseSink;
struct IMAPIAdviseSink;
struct IMsgStore;
struct IMAPISession;
struct IMAPITable;

class MAPIStorageAdviser
{
private:
  std::list<CMAPIAdviseSink*> _sinks;
  IMAPISession *_pIMapiSession;

  IMAPIAdviseSink * _GetSink( CMAPIAdviseSink * pMapiNotifySink );
  IMAPISession * InitIMAPISession();
  IMAPITable * GetStoresTable( IMAPISession* pSession );

public:
  MAPIStorageAdviser(void);
  virtual ~MAPIStorageAdviser(void);
  
  bool AddSink(IMsgStore* pStore);
  bool AdviseAll();

  IMsgStore* _GetStoreFromEntryID( IMAPISession* pSession, ULONG ulStoreBytes, LPBYTE pEntry);

};
