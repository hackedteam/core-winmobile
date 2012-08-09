#pragma once

#include "MAPICommon.h"

class MAPIAttachment
{
private:
  LPATTACH _pAttach;
  LPTSTR _lpFilename;
  LONG _lStatus;
  LONG _cbTotalSize;
  LPBYTE _lpBody;
  DWORD _cbRealSize;

  LPBYTE _GetBody(DWORD* cbSize);

public:
  
  void SetTotalSize(LONG size)
  {
    this->_cbTotalSize = size;
  }
  
  void SetStatus(LONG status)
  {
    this->_lStatus = status;
  }
  
  LPTSTR filename()
  {
    return _lpFilename;
  }
  
  LPBYTE body(DWORD *cbSize)
  {
    if (_lpBody == NULL) 
      _lpBody = _GetBody(&_cbRealSize);

    (*cbSize) = _cbRealSize;
    return _lpBody;
  }
  
  DWORD realSize()
  {
    return _cbRealSize;
  }

  LONG totalSize()
  { 
    return _cbTotalSize;
  }
  
  MAPIAttachment( LPBYTE lpData, DWORD dwSize, LPTSTR lpFilename )
    : _pAttach(NULL), _lpFilename(NULL), _cbRealSize(0), _lStatus(0), _cbTotalSize(0)
  {
    _lpBody = lpData;
    _cbRealSize = dwSize;

    size_t len = _tcslen(lpFilename) + 1;
    _lpFilename = new TCHAR[len];
    ZeroMemory(_lpFilename, len * sizeof(TCHAR));
    wcscpy(_lpFilename, lpFilename);
  }

  MAPIAttachment(LPATTACH pAttach, LPTSTR lpFilename);
  
  virtual ~MAPIAttachment(void);
};
