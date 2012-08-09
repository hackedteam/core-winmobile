#pragma once
#include <string>
using namespace std;

#include <ril.h>
#include "LiveMic.h"

typedef HANDLE HRIL, *LPHRIL;

#include "LiveMic.h"

class RilManager
{				  
private:
	DWORD	m_dwIndex;
	HRIL	m_hRil;

	HRESULT m_hrGetAudio, m_hrSetAudio;
	RILGAININFO m_rilGetInfo;

	static void ResultProc(DWORD dwCode, HRESULT hrCmdID, const void *lpData, DWORD cbdata, DWORD dwParam) {
		((RilManager*)dwParam)->HandleResult(dwCode, hrCmdID, lpData, cbdata); }

	static void NotifyProc(DWORD dwCode, const void *lpData, DWORD cbdata, DWORD dwParam) {
		((RilManager*)dwParam)->HandleNotify(dwCode, lpData, cbdata); }

	void HandleResult(DWORD dwCode, HRESULT hrCmdID, const void *lpData, DWORD cbdata);
	void HandleNotify(DWORD dwCode, const void *lpData, DWORD cbdata);

public:
	BOOL Init();
	BOOL UnInit();
	
	BOOL GetAudioGain();
	BOOL SetAudioGain(BOOL bRestore);

	BOOL HandleCallRing(RILRINGINFO* ri, DWORD cbdata);
	wstring GetRilRemoteCallerNumber(RILREMOTEPARTYINFO* pi);
	BOOL HandleCallerIdEvent(RILREMOTEPARTYINFO* pi, DWORD cbdata);
		 
	BOOL DisableNotification(){ return (RIL_DisableNotifications(m_hRil, RIL_NCLASS_ALL) > 0)?TRUE:FALSE; }
	BOOL EnableNotification(){ return (RIL_EnableNotifications(m_hRil, RIL_NCLASS_ALL) > 0)?TRUE:FALSE; }

	RilManager(void);
	~RilManager(void);
};
