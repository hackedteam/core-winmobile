#pragma once
#include <vector>
using namespace std;

#include <Tapi.h>
#include "LiveMic.h"
#include "VectorUtils.h"

#define  LINEDEVSTATE_ALL 0x1FFFFFF

static void LineCallBack(DWORD hDevice,
						   DWORD dwMessage,
						   DWORD dwInstance,
						   DWORD dwParam1,
						   DWORD dwParam2,
						   DWORD dwParam3);

class TapiManager
{
private:
	static TapiManager*		m_pInstance;
	static volatile LONG	m_lLock;

	BOOL m_bInitialized;
	DWORD m_dwDeviceID, m_dwNumDev;
	HLINEAPP m_hLineApp;
	LINEEXTENSIONID m_extID;
	LINEDEVCAPS* m_lpLineDevCaps;
	LineVector m_vectLines;
	DwordVector m_vectApiVerNegoziated;

	BOOL TapiGetDeviceCAPS(UINT dwDevNum, DWORD * dwApiVerNegoziated);
	BOOL GetCallState(HCALL hCall, LPDWORD lpdwCallStatus);

protected:
	TapiManager(void);

public:
	static TapiManager* self();

	BOOL Init();
	BOOL UnInit();

	BOOL CheckNumber(HCALL hCall, wstring telnum);
	BOOL AnswerCall(HCALL hCall);
	BOOL DropCall(HCALL hCall);
	BOOL DeallocateCall(HCALL hCall);
	
	~TapiManager(void);
};