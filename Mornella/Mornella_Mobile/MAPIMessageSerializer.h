#pragma once

#pragma warning(push, 3)
#include <Windows.h>
#pragma warning(pop)

#include "MAPISerialize.h"

#define CLASS_SMS 	TEXT("IPM.SMSText*")
#define CLASS_MAIL 	TEXT("IPM.Note*")
#define CLASS_MMS 	TEXT("IPM.MMS*")

class CkMime;

class MAPIMessage;
class MAPIAttachment;

enum ObjectTypes {
  STRING_FOLDER            = 0x01000000,
  STRING_CLASS             = 0x02000000,
  STRING_FROM              = 0x03000000,
  STRING_TO                = 0x04000000,
  STRING_CC                = 0x05000000,
  STRING_BCC               = 0x06000000,
  STRING_SUBJECT           = 0x07000000,
  
  HEADER_MAPIV1            = 0x20000000,
  
  OBJECT_MIMEBODY          = 0x80000000,
  OBJECT_ATTACH            = 0x81000000,
  OBJECT_DELIVERYTIME      = 0x82000000,
  OBJECT_TEXTBODY          = 0x84000000,
  
  EXTENDED                 = 0xFF000000,
};

enum VersionFlags {
  MAPI_V1_0_PROTO          = 0x01000000,  // Protocol Version 1
};

enum MessageFlags {
	MESSAGE_INCOMING       = 0x00000001,
};

#pragma pack(4)
struct MAPISerializedMessageHeader {
  DWORD dwSize;             // size of serialized message (this struct + class/from/to/subject + message body + attachs)
  DWORD VersionFlags;       // flags for parsing serialized message
  LONG Status;              // message status
  LONG Flags;               // message flags
  LONG Size;                // message size
  FILETIME DeliveryTime;    // delivery time of message (maybe null)
  DWORD nAttachs;           // number of attachments
};
#pragma pack()

class MAPISerializedMessage {
private:
  CkMime* _ckMime;
  
  bool _DecodeHeader();
  bool _DecodePart( CkMime* part );

  bool _DecodeMail();
  bool _DecodeSMS();
  bool _DecodeMMS();
  
  LPWSTR _from;
  LPWSTR _to;
  LPWSTR _cc;
  LPWSTR _subject;
  LPWSTR _folder;
  LPWSTR _class;

public:
  struct MAPISerializedMessageHeader header;
  
  LPWSTR From() { if (_from) return _from; else return L""; } 
  LPWSTR To() { if (_to) return _to; else return L""; }
  LPWSTR Cc() { if (_cc) return _cc; else return L""; }
  LPWSTR Subject() { if (_subject) return _subject; else return L""; }
  
  void setFrom(LPWSTR from) { _from = from; }
  void setTo(LPWSTR to) { _to = to; }
  void setFolder(LPWSTR folder) { _folder = folder; }
  void setClass(LPWSTR c) { _class = c; }
  void setSubject(LPWSTR subject) { _subject = subject; }
  
  LPWSTR HtmlBody;
  LPWSTR TextBody;
  
  LPBYTE MIMEBody;
  DWORD MIMEBodySize;
  
  MAPISerializedMessage();
  ~MAPISerializedMessage();

  bool Decode();
  void Print();
};

class MAPIMessageSerializer {
private:
  MAPISerializer<ObjectTypes> serializer;
  DWORD _UnserializeEntry( LPBYTE lpPtrToBuffer, MAPISerializedMessage* msg );
  
public:
  MAPISerializedMessage * Unserialize( LPBYTE lpBuffer, DWORD dwLength );
  void PrintHeader( MAPISerializedMessage *msg );
};