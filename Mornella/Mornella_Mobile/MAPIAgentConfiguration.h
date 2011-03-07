#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#include <list>
#include <algorithm>
#pragma warning(pop)

#include "MAPICommon.h"
#include "MAPIAgentClassFilter.h"

enum {
  TAG_LENGTH = 16,
};

enum FilterTypes {
  REALTIME,
  COLLECT,
};

enum ConfigurationObjectTypes {
  CONFIGURATION_TAG     = 0x01000000,
  CONFIGURATION_FILTER  = 0x02000000,
};

class MAPIAgentConfiguration {
private:
  LPWSTR tag;
  std::list<MAPIAgentClassFilter*> collect_filters;
  std::list<MAPIAgentClassFilter*> realtime_filters;

public:

  LPWSTR Tag() { return tag; }
  
  MAPIAgentConfiguration(BOOL bTagged)
    : tag(NULL)
  {
    if (bTagged == TRUE)
    {
      tag = (LPWSTR) LocalAlloc(LPTR, (TAG_LENGTH + 1) * sizeof(WCHAR));
      RandomString(tag, TAG_LENGTH);
    }
  }
  
  ~MAPIAgentConfiguration()
  {
    if (tag) LocalFree(tag);

    std::list<MAPIAgentClassFilter*>::iterator i = collect_filters.begin();
    for (; i != collect_filters.end(); i++)
      if (*i)
        delete *i;
    collect_filters.clear();

    std::list<MAPIAgentClassFilter*>::iterator j = realtime_filters.begin();
    for (; j != realtime_filters.end(); j++)
      if (*j)
        delete *j;
    realtime_filters.clear();
  }
  
  bool HasRealtimeConfig() { return !realtime_filters.empty(); }
  bool HasCollectConfig() { return !collect_filters.empty(); }

  std::list<MAPIAgentClassFilter*>& CollectFilters() { return collect_filters; }
  std::list<MAPIAgentClassFilter*>& RealtimeFilters() { return realtime_filters; }
  
  MAPIAgentClassFilter* CreateFilter(LPTSTR ClassName, FilterTypes Type) 
  {
    _ASSERT(ClassName);

    MAPIAgentClassFilter* filter = new MAPIAgentClassFilter(ClassName, Type); 
    
    switch(Type)
    {
    case REALTIME:
      realtime_filters.push_back(filter);
      break;
    case COLLECT:
      collect_filters.push_back(filter);
      break;
    }
    
    return filter;
  }
  
  BOOL RemoveClassFilter(MAPIAgentClassFilter * filter, FilterTypes Type)
  {
    _ASSERT(filter);

    switch(Type)
    {
    case REALTIME:
      {
        std::list<MAPIAgentClassFilter*>::iterator i = find(realtime_filters.begin(), realtime_filters.end(), filter);
        if (i == realtime_filters.end())
          return FALSE;
        realtime_filters.erase(i);
      }
      break;
    case COLLECT:
      {
        std::list<MAPIAgentClassFilter*>::iterator i = find(collect_filters.begin(), collect_filters.end(), filter);
        if (i == collect_filters.end())
          return FALSE;
        collect_filters.erase(i);
      }
      break;
    }
    
    return TRUE;
  }
  
  static MAPIAgentConfiguration* Unserialize(LPBYTE lpData, DWORD cbSize)
  {
    MAPIAgentConfiguration* config = new MAPIAgentConfiguration(FALSE);
    MAPISerializer<ConfigurationObjectTypes> serializer;
    
    DWORD dwBytesToGo = cbSize;
    LPBYTE ptr = lpData;
    
    while (dwBytesToGo != 0)
    {
    
      if ((LONG)dwBytesToGo < 0)
        exit(1);
      
      DWORD prefix = 0;
      DWORD offset = 0;
      
      offset = serializer.ReadPrefix(ptr, &prefix);
      ptr += offset; dwBytesToGo -= offset;
      
      DWORD type = PREFIX_TYPE(prefix);
      DWORD size = PREFIX_SIZE(prefix);
      
      switch(type)
      {
      case CONFIGURATION_TAG:
        LocalFree(config->tag);
        offset = serializer.UnserializeWString(ptr, size, &config->tag);
        ptr += offset; dwBytesToGo -= offset;
        break;
      case CONFIGURATION_FILTER:
        LPBYTE lpFilter;
        offset = serializer.UnserializeObject(ptr, size, &lpFilter);
        ptr += offset; dwBytesToGo -= offset;
        MAPIAgentClassFilter* filter = MAPIAgentClassFilter::Unserialize(lpFilter, size);

        switch (filter->header.Type)
        {
          case REALTIME:
            config->realtime_filters.push_back(filter);
            break;
          case COLLECT:
            config->collect_filters.push_back(filter);
            break;
        }
        
        LocalFree(lpFilter);
        break;
      }
      
    }
    
    return config;
  }
  
  LPBYTE Serialize(LPDWORD lpdwSize)
  {
    _ASSERT(lpdwSize);
    MAPISerializer<ConfigurationObjectTypes> serializer;
    
    // Allocate buffer
    DWORD cbDataSize = SerializedLength();
    LPBYTE lpData = (LPBYTE) LocalAlloc(LPTR, cbDataSize);
    LPBYTE ptr = lpData;
    
    // serializer header
    ptr += serializer.SerializeWString(ptr, tag, CONFIGURATION_TAG);
    for (std::list<MAPIAgentClassFilter*>::iterator i = collect_filters.begin(); i != collect_filters.end(); i++)
    {
      DWORD dwSerializedFilterLen = 0;
      LPBYTE lpObject = ((MAPIAgentClassFilter*)*i)->Serialize(&dwSerializedFilterLen);
      ptr += serializer.SerializeObject(ptr, lpObject, dwSerializedFilterLen, CONFIGURATION_FILTER);
    }

	for (std::list<MAPIAgentClassFilter*>::iterator i = realtime_filters.begin(); i != realtime_filters.end(); i++)
    {
      DWORD dwSerializedFilterLen = 0;
      LPBYTE lpObject = ((MAPIAgentClassFilter*)*i)->Serialize(&dwSerializedFilterLen);
      ptr += serializer.SerializeObject(ptr, lpObject, dwSerializedFilterLen, CONFIGURATION_FILTER);
    }
        
    (*lpdwSize) = ptr - lpData;
    return lpData;
  }
  
  DWORD SerializedLength()
  {
    MAPISerializer<ConfigurationObjectTypes> serializer;
    
    DWORD cbDataSize = 0;
    cbDataSize += serializer.SerializedWStringLength(tag);
    for (std::list<MAPIAgentClassFilter*>::iterator i = collect_filters.begin(); i != collect_filters.end(); i++)
      cbDataSize += ((MAPIAgentClassFilter*)*i)->SerializedLength();
    
	for (std::list<MAPIAgentClassFilter*>::iterator i = realtime_filters.begin(); i != realtime_filters.end(); i++)
      cbDataSize += ((MAPIAgentClassFilter*)*i)->SerializedLength();

    return cbDataSize;
  }

  MAPIAgentClassFilter* MatchClassName(LPWSTR ClassName, DWORD FilterType)
  {
    std::list<MAPIAgentClassFilter*>* list = NULL;

    switch (FilterType)
    {
    case COLLECT:
      list = &collect_filters;
      break;
    case REALTIME:
      list = &realtime_filters;
      break;
    }

    for (
      std::list<MAPIAgentClassFilter*>::iterator i = list->begin();
      i != list->end(); 
    i++
      )
    {
      MAPIAgentClassFilter * filter = *i;
      if ( TRUE == CmpWildW( filter->header.MessageClass, ClassName ) )
        return filter;
    }

    return NULL;
  }
};
