#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#include <mapiutil.h>
#include <string.h>
#include <time.h>
#pragma warning(pop)

#ifndef _ASSERT
#define _ASSERT(x) ASSERT(x)
#endif

enum {
  MAXBUF = 256
};

struct rawEntryID {
	ULONG cbSize;
	LPBYTE lpEntryID;
};

#ifndef DEBUG
#undef OutputDebugString
#define OutputDebugString(...)
#endif

#define ASCII_PRINTABLE_LOWER 33
#define ASCII_PRINTABLE_UPPER 126

inline void RandomString( LPWSTR lpString, DWORD cchLength )
{
  _ASSERT(lpString);
  
  LPWSTR alphabet = L"abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+:~";
  
  for (DWORD i = 0; i < cchLength; i++)
    lpString[i] = alphabet[rand() % wcslen(alphabet)];
}

inline void DumpToFile( LPTSTR filepath, LPBYTE pszBodyInBytes, DWORD cbBody )
{
  HANDLE hFile = CreateFile(filepath, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    DWORD cbWritten = 0;
    HRESULT hr = WriteFile(hFile, pszBodyInBytes, cbBody, &cbWritten, NULL);
    if (FAILED(hr))
    {

	}
    CloseHandle(hFile);
  } else {

  }
}

// Compara due stringhe con wildcard, torna 0 se le stringhe sono diverse
inline INT CmpWildW(WCHAR *wild, WCHAR *string) {
  WCHAR *cp = NULL, *mp = NULL;

  while ((*string) && (*wild != '*')) {
    if ((towupper((WCHAR)*wild) != towupper((WCHAR)*string)) && (*wild != '?')) {
      return 0;
    }
    wild++;
    string++;
  }

  while (*string) {
    if (*wild == '*') {
      if (!*++wild) {
        return 1;
      }

      mp = wild;
      cp = string+1;
    } else if ((towupper((WCHAR)*wild) == towupper((WCHAR)*string)) || (*wild == '?')) {
      wild++;
      string++;
    } else {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == '*') {
    wild++;
  }

  return !*wild;
}

#ifdef UNDER_CE
inline time_t time( time_t *inTT ) { 
  SYSTEMTIME sysTimeStruct; 
  FILETIME fTime; 
  ULARGE_INTEGER int64time; 
  time_t locTT = 0; 

  if ( inTT == NULL ) { 
    inTT = &locTT; 
  } 

  GetSystemTime( &sysTimeStruct ); 
  if ( SystemTimeToFileTime( &sysTimeStruct, &fTime ) ) { 
    memcpy( &int64time, &fTime, sizeof( FILETIME ) ); 
    /* Subtract the value for 1970-01-01 00:00 (UTC) */ 
    int64time.QuadPart -= 0x19db1ded53e8000; 
    /* Convert to seconds. */ 
    int64time.QuadPart /= 10000000; 
    *inTT = static_cast<time_t>(int64time.QuadPart); 
  } 
  
  return *inTT; 
}
#endif