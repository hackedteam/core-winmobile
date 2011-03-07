#pragma once

#include "MAPISerialize.h"

enum MAPIMarkupType {
  MAPIMARKUP_COLLECT_KEYWORD  = 0x01000000,
  MAPIMARKUP_COLLECT_FROMDATE = 0x02000000,
};

class MAPIMarkup
{
private:
  LPWSTR _lpwCollectKeyword;
  FILETIME _ftDate;
  
public:
  MAPIMarkup(void);
  virtual ~MAPIMarkup(void);

  bool IsCollectCompletedForKeyword(LPWSTR lpwKeyword);
  bool SetKeyword(LPWSTR lpwKeyword);
  bool SetDate(FILETIME ftDate);
  bool Write();
  bool Read();
  bool IsMarkup();

  LPWSTR CollectKeyword()
  {
    return _lpwCollectKeyword;
  }

  FILETIME FromDate()
  {
    return _ftDate;
  }
};
