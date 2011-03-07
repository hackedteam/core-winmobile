#ifndef __MAPIRULE_H__
#define __MAPIRULE_H__

#include "IPCQueue.h"

// {DD69A982-6C70-4a55-8BE4-6B32A1F9A527}
// extern DEFINE_GUID(CLSID_TMapiRule, 0xdd69a982, 0x6c70, 0x4a55, 0x8b, 0xe4, 0x6b, 0x32, 0xa1, 0xf9, 0xa5, 0x27);

/* Add to apps.reg:
[HKEY_CLASSES_ROOT\CLSID\{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}\InProcServer32]
@="tmail.dll"
*/

/*
Add this line too:
[HKEY_LOCAL_MACHINE\Software\Microsoft\Inbox\Svc\SMS\Rules]
"{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}"=dword:1
*/

enum {
	DBG_CRITICAL = 1,
	DBG_HIGH = 2,
	DBG_NORMAL = 3,
	DBG_LOW = 4,
	DBG_NOTIFY = 5,
	DBG_VERBOSE = 6
};

#ifdef _DEBUG
#undef DBG_TRACE
#define DBG_TRACE(msg, prior, err) DebugTrace(msg, __FILEW__, __FUNCTIONW__, __LINE__, prior, err)
#undef DBG_TRACE_INT
#define DBG_TRACE_INT(msg, prior, err, val) DebugTraceInt(msg, __FILEW__, __FUNCTIONW__, prior, err, val)
#else
#undef DBG_TRACE
#define DBG_TRACE(msg, prior, err)
#define DBG_TRACE_INT(msg, prior, err, val)
#endif

void DebugTrace(const PWCHAR pwMsg, const PWCHAR pwFileName, const PWCHAR pwFunction, const UINT uLine, UINT uPriority, BOOL bLastError);

//*********************************************************************
//Class CMailRuleClient - Implementation of IMailRuleClient
//
//Inheritance:
//	IMailRuleClient
//	IUnknown
//
//Purpose:
//	This class serves as implementation for the IMailRuleClient
//	interface and provides our Rule Client functionality.
//
//**********************************************************************
class CMailRuleClient : public IMailRuleClient
{
	private:
		long	m_cRef;	// reference counter!
		TCHAR	m_wszServiceProvider[32];
		bool	m_cInitialized;
    
    IPCQueue* m_qRead;
    IPCQueue* m_qWrite;
    
	protected:
		STDMETHOD (DeleteMessage)(IMsgStore *pMsgStore, IMessage *pMsg, ULONG cbMsg, LPENTRYID lpMsg, ULONG cbDestFolder, LPENTRYID lpDestFolder, ULONG *pulEventType, MRCHANDLED *pHandled);

	public:
		CMailRuleClient();
		~CMailRuleClient();

		// IUnknown
		STDMETHOD (QueryInterface)(REFIID iid, LPVOID *ppv);
		STDMETHOD_(ULONG, AddRef)();
		STDMETHOD_(ULONG, Release)();

		// IMailRuleClient
		MAPIMETHOD(Initialize)(IMsgStore *pMsgStore, MRCACCESS *pmaDesidered);
		MAPIMETHOD(ProcessMessage)(IMsgStore *pMsgStore, ULONG cbMsg, LPENTRYID lpMsg, ULONG cbDestFolder, LPENTRYID lpDestFolder, ULONG *pulEventType, MRCHANDLED *pHandled);
};

#endif