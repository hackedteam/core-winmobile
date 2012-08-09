#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#include <msgqueue.h>
#pragma warning(pop)

#include "IPCMsg.h"

#define MICRO_SLEEP 350
#define MAX_TIMEOUT 3 * 60 * 1000 // 3 Minuti di attesa massima

class IPCQueue
{
private:
  HANDLE hQueue;
  MSGQUEUEOPTIONS qOpts;
  LPWSTR lpwName;
  
public:
  
  IPCQueue(LPWSTR Name, bool bRead)
    : hQueue(INVALID_HANDLE_VALUE), lpwName(NULL)
  {
    ZeroMemory(&qOpts, sizeof(qOpts));
    
    qOpts.dwSize = sizeof(MSGQUEUEOPTIONS);
    qOpts.dwFlags = MSGQUEUE_NOPRECOMMIT | MSGQUEUE_ALLOW_BROKEN;
    qOpts.cbMaxMessage = 1024 * 4;
    qOpts.dwMaxMessages = 0;
    qOpts.bReadAccess = FALSE;
    if (bRead)
      qOpts.bReadAccess = TRUE;
    
    lpwName = wcsdup(Name);
  }
  
  virtual ~IPCQueue(void)
  {
    if (lpwName)
      free(lpwName);

    if (hQueue != INVALID_HANDLE_VALUE)
      CloseMsgQueue(hQueue);
  }
  
  bool Open()
  {
    hQueue = CreateMsgQueue(lpwName, &qOpts);
    if (hQueue == NULL)
    {
     // WCHAR str[256] = {0};
      //wsprintf(str, L"WriteMsgQueue ERROR %d", GetLastError());
      // MessageBox(NULL, str, L"DEBUG", MB_OK);
      return false;
    }
    
    return true;
  }
  
  LPBYTE Read(LPBYTE lpBuf, DWORD cbSize, LPDWORD lpcbNumberOfBytesRead, DWORD dwTimeout = 0)
  { 
    if (lpBuf == NULL)
    {
      (*lpcbNumberOfBytesRead) = 0;
      return NULL;
    }
    
    DWORD dwFlags = 0;
    BOOL bOk = ReadMsgQueue(hQueue, lpBuf, cbSize, lpcbNumberOfBytesRead, dwTimeout, &dwFlags);
    if (bOk == FALSE)
    {
      DWORD dwErr = GetLastError();

      switch (dwErr)
      {
      case ERROR_TIMEOUT:
        break;
      default:
       // wprintf(L"ReadMsgQueue failed [ERROR %08x]\n", GetLastError());
        break;
      }
      
      (*lpcbNumberOfBytesRead) = 0;
    }
    
    return lpBuf;
  }
  
  bool Write(LPBYTE lpData, DWORD cbSize, DWORD dwTimeout = 1000)
  {
    if (lpData == NULL)
      return false;
    
    // MessageBox(NULL, L"Writing message to queue.", L"DEBUG", MB_OK);
    
    BOOL bOk = WriteMsgQueue(hQueue, lpData, cbSize, dwTimeout, 0);
    if (bOk == FALSE)
    {
      //WCHAR str[256] = {0};
      //wsprintf(str, L"WriteMsgQueue ERROR %d", GetLastError());
      // MessageBox(NULL, str, L"DEBUG", MB_OK);
      return false;
    }

    // MessageBox(NULL, L"Message written to queue.", L"DEBUG", MB_OK);
    
    return true;
  }
  
  IPCMsg* WaitForReply()
  {
	  DWORD cbNumberOfBytesRead = 0;
	  LPBYTE lpData = (LPBYTE) LocalAlloc(LPTR, 1024);

	  for (UINT i = 0; i < MAX_TIMEOUT / MICRO_SLEEP; i++) // 10 seconds of waiting
	  {
		  lpData = Read(lpData, 1024, &cbNumberOfBytesRead);

		  if (cbNumberOfBytesRead != 0)
			  break;

		  Sleep(MICRO_SLEEP);
	  }

	  if (lpData == NULL)
		  return NULL;

	  return IPCMsg::UnserializeReply(lpData, cbNumberOfBytesRead);
  }
  
  bool WriteMessage(IPCMsg* message)
  {
    DWORD cbSize = 0;
    LPBYTE lpData = message->Serialize(&cbSize);
    if (lpData == NULL)
      return false;
    
    for (UINT i = 0; i < 20; i++)
    {
      if (Write(lpData, cbSize) == true)
      {
        delete[] lpData;
        return true;
      }
      Sleep(MICRO_SLEEP);
    }
    
    delete[] lpData;
    return false;
  }
};
