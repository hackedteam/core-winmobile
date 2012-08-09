#pragma once

#include "Common.h"
#include "Conf.h"

enum MsgType {
  MSG_REPLY,
  MSG_SMS
};

class IPCMsg
{
private:
  LPWSTR _lpwPhoneNumber;
  LPWSTR _lpwMessage;
  
  DWORD _dwReply;

  MsgType _type;

  IpcMsg* _header;
  
protected:
  
  LPBYTE _SerializeReply(LPDWORD lpcbSize)
  {
    IpcMsg* msg = (IpcMsg*) new(std::nothrow) IpcMsg;
    if (msg == NULL)
      return NULL;
    
    msg->dwForwarded = 0;
    msg->dwMsgLen = sizeof(DWORD);
    msg->dwSender = EVENT_SMS;
    msg->dwRecipient = EVENT_SMS;
    memcpy(msg->Msg, &_dwReply, sizeof(_dwReply));
    
    // LPWSTR lpwStr = new WCHAR[256];
    // wsprintf(lpwStr, L"[%s] Reply code [%08x:%08x]", __FUNCTIONW__, _dwReply, (DWORD)(*msg->Msg));
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);
    
    (*lpcbSize) = sizeof(IPCMsg);
    
    return (LPBYTE)msg;
  }
  
  LPBYTE _SerializeSMS(LPDWORD lpcbSize)
  {
    DWORD cbSize = 
      sizeof(IpcMsg)                                // header
      + 2 * sizeof(DWORD)                           // lengths of phone and message strings
      + wcslen(_lpwPhoneNumber) * sizeof(WCHAR)     // phone number
      + wcslen(_lpwMessage) * sizeof(WCHAR)         // message
      + 2 * sizeof(WCHAR);                          // null terminators

    // LPWSTR lpwStr = new WCHAR[1024];
    // wsprintf(lpwStr, L"Msg size %d", cbSize);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);

    LPBYTE lpData = (LPBYTE) new(std::nothrow) WCHAR[cbSize];
    if (lpData == NULL)
      return NULL;

    IpcMsg* header = (IpcMsg*)lpData;
    // wsprintf(lpwStr, L"header @ %08x", header);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);

    header->dwForwarded = 0;
    header->dwMsgLen = cbSize - sizeof(IpcMsg);
    header->dwSender = 0x2002;
    header->dwRecipient = 0x2002;

    LPBYTE ptr = header->Msg;

    // wsprintf(lpwStr, L"header->SizeNumber @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);
  
    // Size of Phone number
    DWORD cbPhoneNumber = (DWORD)wcslen(_lpwPhoneNumber) * sizeof(WCHAR) + sizeof(WCHAR); // without NULL terminator
    memcpy(ptr, &cbPhoneNumber, sizeof(DWORD));
    ptr += sizeof(DWORD);

    // wsprintf(lpwStr, L"header->PhoneNumber @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);

    // Phone number string
    StringCchCopy((LPWSTR)ptr, wcslen(_lpwPhoneNumber) + 1, _lpwPhoneNumber);
    ptr += cbPhoneNumber;

    // wsprintf(lpwStr, L"header->SizeMessage @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);

    // Size of Message
    DWORD cbMessage = (DWORD)wcslen(_lpwMessage) * sizeof(WCHAR) + sizeof(WCHAR); // without NULL terminator
    memcpy(ptr, &cbMessage, sizeof(DWORD));
    ptr += sizeof(DWORD);
    
    // wsprintf(lpwStr, L"header->Message @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);
    
    // Message string
    StringCchCopy((LPWSTR)ptr, wcslen(_lpwMessage) + 1, _lpwMessage);
    ptr += cbMessage;
    
    // wsprintf(lpwStr, L"header->End @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);
    
    (*lpcbSize) = ptr - lpData;
    
    // wsprintf(lpwStr, L"header->End @ %08x", ptr);
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);

    return lpData;
  }

public:

  MsgType Type() { return _type; }
  LPWSTR PhoneNumber() { return _lpwPhoneNumber; }
  LPWSTR Message() { return _lpwMessage; }
  DWORD Reply() { /*WCHAR lpwStr[256]; wsprintf(lpwStr, L"REPLY %08x", _dwReply); MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);*/ return _dwReply; }

  void SetHeader(IpcMsg* header) { memcpy(_header, header, sizeof(IpcMsg)); }
  void SetReply(DWORD dwReply) { _dwReply = dwReply; }
  
  IPCMsg(LPWSTR PhoneNumber, LPWSTR Message)
    : _lpwPhoneNumber(NULL), _lpwMessage(NULL), _header(NULL), _dwReply(IPC_PROCESS)
  {
    _type = MSG_SMS;
    _lpwPhoneNumber = wcsdup(PhoneNumber);
    _lpwMessage = wcsdup(Message);

    _header = new(std::nothrow) IpcMsg;
  }
  
  IPCMsg(DWORD Reply)
    : _lpwPhoneNumber(NULL), _lpwMessage(NULL), _header(NULL), _dwReply(IPC_PROCESS)
  {
    _type = MSG_REPLY;
    _dwReply = Reply;

    _header = new(std::nothrow) IpcMsg;
  }
  
  virtual ~IPCMsg(void)
  {
    if (_lpwMessage)
      free(_lpwMessage);

    if (_lpwPhoneNumber)
      free(_lpwPhoneNumber);

    if (_header)
      delete _header;
  }
  
  LPBYTE Serialize(LPDWORD lpcbSize)
  {
    switch(_type)
    {
    case MSG_REPLY:
      return _SerializeReply(lpcbSize);
      break;
    case MSG_SMS:
      return _SerializeSMS(lpcbSize);
      break;
    }

    return NULL;
  }
  
  bool ToBeForwarded(LPBYTE lpData, DWORD cbSize)
  {
    IpcMsg* header = (IpcMsg*)lpData;
    
    if (header->dwRecipient != 0x2002)
      return true;

    return false;
  }
  
  static IPCMsg* UnserializeReply(LPBYTE lpData, DWORD cbSize)
  {
    if (lpData == NULL)
      return NULL;
    
    if (cbSize < sizeof(IpcMsg)) // message too short
      return NULL;
    
    // wprintf(L"[%s] sizeof(IpcMsg) == %d, cbSize == %d [diff %d]\n", __FUNCTIONW__, sizeof(IpcMsg), cbSize, cbSize - sizeof(IpcMsg));
    
    IpcMsg header;
    memcpy(&header, lpData, sizeof(IpcMsg));
    
    //WCHAR lpwStr[256];
    //wsprintf(lpwStr, L"[%s] REPLY %08x", __FUNCTIONW__, (DWORD)(*header.Msg));
    // MessageBox(NULL, lpwStr, L"DEBUG", MB_OK);
    
    return new IPCMsg((DWORD)(*header.Msg));
  }
};
