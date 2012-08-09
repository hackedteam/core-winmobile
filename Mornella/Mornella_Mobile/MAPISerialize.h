#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#pragma warning(pop)

#define PREFIX_TYPE(prefix) ((prefix) & ~TYPE_MASK)
#define PREFIX_SIZE(prefix) ((prefix) & TYPE_MASK)

enum {
  TYPE_MASK = 0x00FFFFFF,
};

template <typename T>
class MAPISerializer
{
public:
  MAPISerializer(void) {}
  virtual ~MAPISerializer(void) {}
  
  DWORD Prefix( DWORD dwLength, T type)
  {
    DWORD prefix = dwLength;
    prefix &= TYPE_MASK;    // clear type bits
    prefix |= (DWORD)type; 

    return prefix;
  }
  
  DWORD ReadPrefix(LPBYTE lpData, LPDWORD lpdwPrefix)
  {
     ASSERT(lpdwPrefix);
     CopyMemory(lpdwPrefix, lpData, sizeof(DWORD));

     return sizeof(DWORD);
  }
  
  DWORD SerializeString( LPBYTE lpDest, LPSTR lpString, T type)
  {
    ASSERT(lpDest);
    DWORD cbStringLength = 0;

    if (lpString == NULL)
      return 0;

    cbStringLength = strlen(lpString);
    if (cbStringLength == 0)
      return 0;

    return SerializeObject(lpDest, (LPBYTE)lpString, cbStringLength, type);
  }
  
  DWORD UnserializeString( LPBYTE lpSrc, DWORD cbSrcSize, LPSTR* lpDest )
  {
    ASSERT( lpSrc );
    ASSERT( cbSrcSize > 0 );
    
    // account for NULL terminator
    (*lpDest) = (LPSTR) LocalAlloc(LPTR, cbSrcSize + 1);
    if (*lpDest == NULL)
      return NULL;
    
    CopyMemory(lpDest, lpSrc, cbSrcSize);
    
    return cbSrcSize;
  }
  
  DWORD SerializeWString( LPBYTE lpDest, LPTSTR lpString, T type )
  {
    ASSERT(lpDest);
    
    DWORD cbStringLength = 0;
    
    // if string is invalid, ignore and return 0 as length
    if (lpString == NULL)
      return 0;
    
    // if string length == 0, ignore and return 0 as length
    cbStringLength = wcslen(lpString) * sizeof(WCHAR);
    if (cbStringLength == 0)
      return 0;

    LPBYTE ptr = lpDest;

    // copy prefix and string *** WITHOUT NULL TERMINATOR ***
    DWORD prefix = Prefix(cbStringLength, type);
    CopyMemory(ptr, &prefix, sizeof(prefix));
    ptr += sizeof(prefix);
    CopyMemory(ptr, lpString, cbStringLength);
    ptr += cbStringLength;
    
    return ptr - lpDest;
  }

  DWORD UnserializeWString( LPBYTE lpSrc, DWORD cbSrcSize, LPWSTR* lpDest )
  {
    ASSERT( lpSrc );
    ASSERT( cbSrcSize > 0 && ( (cbSrcSize % sizeof(TCHAR)) == 0 ) );  // check for correct size of Unicode strings
    
    // account for NULL terminator
    (*lpDest) = (LPWSTR) LocalAlloc(LPTR, cbSrcSize + (1 * sizeof(WCHAR)));
    if (*lpDest == NULL)
      return NULL;
    
    CopyMemory(*lpDest, lpSrc, cbSrcSize);
    
    return cbSrcSize;
  }
  
  DWORD SerializeObject( LPBYTE lpDest, LPBYTE lpObject, DWORD cbObjectSize, T type )
  {
    ASSERT( lpDest );

    if (cbObjectSize == 0)
      return 0;

    DWORD prefix = Prefix(cbObjectSize, type);
    CopyMemory(lpDest, &prefix, sizeof(prefix));
    CopyMemory(lpDest + sizeof(prefix), lpObject, cbObjectSize);
    
    return sizeof(prefix) + cbObjectSize;
  }
  

  DWORD UnserializeObject( LPBYTE lpSrc, DWORD cbSrcSize, LPBYTE* lpDest)
  {
    ASSERT( lpSrc );
    ASSERT( lpDest );
    ASSERT( cbSrcSize > 0 );
    
    (*lpDest) = (LPBYTE) LocalAlloc(LPTR, cbSrcSize);
    CopyMemory(*lpDest, lpSrc, cbSrcSize);
    
    return cbSrcSize;
  }


  inline DWORD SerializedWStringLength( LPTSTR lpString )
  {
    // if string is null
    if (lpString == NULL)
      return 0;
    
    // account for string length field
    DWORD cbStringLength = 0;
    cbStringLength += sizeof(DWORD);
    cbStringLength += wcslen(lpString) * sizeof(WCHAR);

    return cbStringLength;
  }
  
  inline DWORD SerializedStringLength ( LPSTR lpString )
  {
    if (lpString == NULL)
      return 0;
    else if (strlen(lpString) == 0)
      return 0;

    DWORD cbStringLength = 0;
    cbStringLength += sizeof(DWORD);
    cbStringLength += strlen(lpString);

    return cbStringLength;
  }
  
  inline DWORD SerializedObjectLength( DWORD dwObjectLength )
  {
    if (dwObjectLength == 0) return 0;
    return dwObjectLength + sizeof(DWORD);
  }
};
