#include "MAPIAttachment.h"

MAPIAttachment::MAPIAttachment(LPATTACH pAttach, LPTSTR lpFilename)
: _pAttach(pAttach), _lpFilename(NULL), _lpBody(NULL), _cbRealSize(0), _lStatus(0), _cbTotalSize(0)
{
  size_t len = _tcslen(lpFilename) + 1;
  _lpFilename = new TCHAR[len];
  ZeroMemory(_lpFilename, len * sizeof(TCHAR));
  wcscpy(_lpFilename, lpFilename);
}

MAPIAttachment::~MAPIAttachment(void)
{
  if (_lpBody)
    LocalFree(_lpBody);

  delete[] _lpFilename;

  if (_pAttach)
    _pAttach->Release();
}

LPBYTE MAPIAttachment::_GetBody(DWORD* cbSize)
{
  ASSERT(_pAttach);
  
  HRESULT hr = E_FAIL;
  LPBYTE lpBody = NULL;

  SPropTagArray tagArray; 
  tagArray.cValues = 1; 
  tagArray.aulPropTag[0] = PR_ATTACH_DATA_BIN;
  
  LPSTREAM pStream = NULL;
  hr = _pAttach->OpenProperty(PR_ATTACH_DATA_BIN, (LPIID)&IID_IStream, STGM_READ, 0, (LPUNKNOWN*)&pStream);
  if (FAILED(hr))
  {
    return NULL;
  }
  
  STATSTG stsg;
  hr = pStream->Stat(&stsg, STATFLAG_NONAME);
  if (FAILED(hr))
  {
    pStream->Release();
    return NULL;
  }
  
  (*cbSize) = (DWORD)stsg.cbSize.LowPart;
  
  if (_cbRealSize > 0)
  {
    lpBody = (LPBYTE)LocalAlloc(LPTR, *cbSize);

    ULONG cbRead = 0;
    hr = pStream->Read(lpBody, *cbSize, &cbRead);
    if (FAILED(hr))
    {
      LocalFree(lpBody);
      pStream->Release();
      return NULL;
    }
  }
  
  pStream->Release();
  
  return lpBody;
}
