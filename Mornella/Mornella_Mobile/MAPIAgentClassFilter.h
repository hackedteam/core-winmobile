#pragma once

#pragma warning(push, 3)
#include <windows.h>
#include <strsafe.h>
#include <list>
#pragma warning(pop)

#include "MAPISerialize.h"

#define CLASS_SMS 	TEXT("IPM.SMSText*")
#define CLASS_MAIL 	TEXT("IPM.Note*")
#define CLASS_MMS 	TEXT("IPM.MMS*")

enum {
  MAPIAGENTCONF_V1_0    = 0x00000001,
  MAPIAGENTCONF_CLASSNO = 6,            // number of message classes present in conf struct
  MAPIAGENTCONF_CLASSNAMELEN = 32,
  MAPIAGENTCONF_HEADER_V1_0 = MAPIAGENTCONF_V1_0,
};

#define FILTER_LOGWHOLEMESSAGE   0
#define FILTER_DONOTFILTERBYSIZE 0

enum FilterObjectTypes {
  FILTER_KEYWORDS     = 0x01000000,
  FILTER_CONF_V1_0    = 0x20000000,
  FILTER_CLASS_V1_0   = 0x40000000,
};

class MAPIAgentClassFilter 
{
public:
#pragma pack(4)
  struct MAPIAgentClassFilterHeader {
    DWORD Size;
    DWORD Version;
    
    DWORD Type;
    
    TCHAR MessageClass[MAPIAGENTCONF_CLASSNAMELEN];   // Message class, may use "*" wildcard 
    
    BOOL Enabled;                                     // FALSE for disabled or non configured classes, otherwise TRUE
    BOOL All;                                         // take all messages of this class
    
    BOOL DoFilterFromDate;                            // get messages delivered past this date
    FILETIME FromDate;                  
    
    BOOL DoFilterToDate;                              // get messages delivered to this date
    FILETIME ToDate;                    
    
    LONG MaxMessageSize;                              // filter by message size, 0 means do not filter by size
    LONG MaxMessageBytesToLog;                        // get only this max bytes for each message, 0 means take whole message
  } header;
#pragma pack()
  
  std::list<LPTSTR> KeywordSearches; 
  
  MAPIAgentClassFilter() {}
  MAPIAgentClassFilter(LPTSTR ClassName, DWORD Type)
  {
    ZeroMemory(&header, sizeof(header));
    header.Enabled = TRUE;
    header.Version = FILTER_CLASS_V1_0;
    header.Type = Type;
    StringCchCopy(header.MessageClass, MAPIAGENTCONF_CLASSNAMELEN, ClassName);
  }
  
  ~MAPIAgentClassFilter()
  {
    for(std::list<LPTSTR>::iterator i = KeywordSearches.begin(); i != KeywordSearches.end(); i++)
    {
      LPTSTR str = *i;
      if (str)
        LocalFree(str);
    }
	KeywordSearches.clear();
  }
  
  BOOL Enabled() { return header.Enabled; }
  BOOL All() { return header.All; }
  
  DWORD Type() { return header.Type; }
  LPWSTR Class() { return (LPWSTR)header.MessageClass; }
  
  BOOL DoFilterFromDate() { return header.DoFilterFromDate; }
  FILETIME FromDate() { return header.FromDate; }
  void SetFromDate(FILETIME* ftFromDate)
  {
    _ASSERT(ftFromDate);
    CopyMemory(&header.FromDate, ftFromDate, sizeof(FILETIME));
    header.DoFilterFromDate = TRUE;
  }
  
  BOOL DoFilterToDate() { return header.DoFilterToDate; }
  FILETIME ToDate() { return header.ToDate; }
  void SetToDate(FILETIME* ftToDate)
  {
    _ASSERT(ftToDate);
    CopyMemory(&header.ToDate, ftToDate, sizeof(FILETIME));
    header.DoFilterToDate = TRUE;
  }
  
  LONG MaxMessageSize() { return header.MaxMessageSize; }
  void SetMaxMessageSize(DWORD dwMaxSize)
  {
    header.MaxMessageSize = dwMaxSize;
  }
  
  LONG MaxMessageBytesToLog() { return header.MaxMessageBytesToLog; }
  void SetMaxMessageBytesToLog(DWORD dwBytesToLog)
  {
    header.MaxMessageBytesToLog = dwBytesToLog;
  }
	
  bool HasKeyword() 
  { 
    if (header.All) 
      return true; 
    else 
      return !KeywordSearches.empty(); 
  }
  
  LPWSTR Keyword() 
  {
    if (header.All)
      return L"*";
    else
      return (LPWSTR)(KeywordSearches.front()); 
  }
  
  void AddKeywordsSearch(LPTSTR keywords)
  {
    _ASSERT(keywords);
	LPWSTR lpKeyword = (LPWSTR) LocalAlloc(LPTR, (wcslen(keywords) + 1) * sizeof(WCHAR)); 
    StringCchCopy(lpKeyword, wcslen(keywords) + 1, keywords);
	KeywordSearches.push_back(lpKeyword);
  }
  
  LPBYTE Serialize(LPDWORD lpdwLength)
  {
    _ASSERT(lpdwLength);
    
    MAPISerializer<FilterObjectTypes> serializer;
    
    // allocate buffer
    DWORD cbDataSize = SerializedLength();
    LPBYTE lpData = (LPBYTE) LocalAlloc(LPTR, cbDataSize);
    LPBYTE ptr = lpData;
    
    // serialize header
    ptr += serializer.SerializeObject(ptr, (LPBYTE)&header, sizeof(header), FILTER_CLASS_V1_0);
    // serialize keyword search strings
    for (std::list<LPTSTR>::iterator i = KeywordSearches.begin(); i != KeywordSearches.end(); i++)
      ptr += serializer.SerializeWString(ptr, (*i), FILTER_KEYWORDS);
    
    (*lpdwLength) = ptr - lpData;
    
    return lpData;
  }
  
  DWORD SerializedLength()
  {
    MAPISerializer<FilterObjectTypes> serializer;
    DWORD cbDataSize = 0;
    cbDataSize += serializer.SerializedObjectLength(sizeof(header));
    for (std::list<LPTSTR>::iterator i = KeywordSearches.begin(); i != KeywordSearches.end(); i++)
      cbDataSize += serializer.SerializedWStringLength((*i));
      
    return cbDataSize + sizeof(DWORD);
  }
  
  static MAPIAgentClassFilter* Unserialize(LPBYTE lpData, DWORD dwSize)
  {
    _ASSERT(lpData);
    _ASSERT(dwSize > 0);
    
    MAPISerializer<FilterObjectTypes> serializer;
    MAPIAgentClassFilter* filter = new MAPIAgentClassFilter();
    
    LPBYTE ptr = lpData;
    DWORD dwBytesToGo = dwSize;
    
    while (dwBytesToGo != 0)
    {
      DWORD prefix = 0;
      ptr += serializer.ReadPrefix(ptr, &prefix);
      
      DWORD type = PREFIX_TYPE(prefix);
      DWORD size = PREFIX_SIZE(prefix);
      
      dwBytesToGo -= sizeof(prefix) + size;
            
      switch(type)
      {
      case FILTER_CLASS_V1_0:
        {
          DWORD dwHeaderSize = sizeof(filter->header);
          if (size == dwHeaderSize)
          {
            LPBYTE lpObject = NULL;
            ptr += serializer.UnserializeObject(ptr, size, (LPBYTE*)&lpObject);
            CopyMemory(&filter->header, lpObject, size);
            if (lpObject)
              LocalFree(lpObject);
          }
        }
        break;
      case FILTER_KEYWORDS:
        {
          LPTSTR keyword;
          ptr += serializer.UnserializeWString(ptr, size, &keyword);
          filter->AddKeywordsSearch(keyword);
        }
        break;
      } /* switch */
    } /* while */

    
        
    return filter;
  }
};

