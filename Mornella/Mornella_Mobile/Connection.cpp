#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"

#include <connmgr.h>
#include <Connmgr_status.h>
#include <Iptypes.h>
#include <Iphlpapi.h>
#include <tapi.h>
#include <pm.h>

DWORD WINAPI OnConnection(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	Configuration *conf;
	wstring subType;
	int delay, iterations, curIterations = 0, waitDelay = 0;
	HANDLE eventHandle, hConnManager = INVALID_HANDLE_VALUE;
	HRESULT hResult;
	MSG msg;
	BOOL connected = FALSE;

	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		delay = conf->getInt(L"delay") * 1000;
	} catch (...) {
		delay = INFINITE;
	}

	try {
		iterations = conf->getInt(L"iter");
	} catch (...) {
		iterations = MAXINT;
	}

	// Attendiamo che il ConnectionManager sia pronto
	hConnManager = ConnMgrApiReadyEvent();

	if (hConnManager == NULL || hConnManager == INVALID_HANDLE_VALUE) {
		DBG_TRACE(L"Debug - Connection.cpp - Connection FAILED [Connection Manager not present]\n", 5, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	if (WaitForSingleObject(hConnManager, 5000) != WAIT_OBJECT_0) {
		CloseHandle(hConnManager);
		DBG_TRACE(L"Debug - Connection.cpp - Connection FAILED [Connection Manager not ready]\n", 5, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	CloseHandle(hConnManager);

	HWND hWin = CreateWindow(L"STATIC", L"Window", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

	if (hWin == NULL) {
		DBG_TRACE(L"Debug - Connection.cpp - Connection FAILED [Cannot create notification window]\n", 5, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	UINT uStatusChange = RegisterWindowMessage(CONNMGR_STATUS_CHANGE_NOTIFICATION_MSG);

	if (uStatusChange == 0) {
		DestroyWindow(hWin);
		DBG_TRACE(L"Debug - Connection.cpp - Connection FAILED [Cannot register window message]\n", 5, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	hResult = ConnMgrRegisterForStatusChangeNotification(TRUE, hWin);

	if (hResult != S_OK) {
		DestroyWindow(hWin);
		DBG_TRACE(L"Debug - Connection.cpp - Connection FAILED [Cannot register for notifications]\n", 5, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	DBG_TRACE(L"Debug - Connection.cpp - Connection Event started\n", 5, FALSE);

	LOOP {
		int now = GetTickCount();

		if (PeekMessage(&msg, hWin, 0, 0, PM_REMOVE)) {
			if (msg.message == uStatusChange) {
				switch (msg.wParam) {
					case CONNMGR_STATUS_CONNECTED:
						WaitForSingleObject(eventHandle, 40000);

						if (me->shouldStop()) {
							ConnMgrRegisterForStatusChangeNotification(FALSE, hWin);
							DestroyWindow(hWin);
							me->setStatus(EVENT_STOPPED);
							DBG_TRACE(L"Debug - Connection.cpp - Connection clean stop [0]\n", 5, FALSE);
							return 0;
						}

						DBG_TRACE(L"Debug - Connection.cpp - Connection triggered [In Action]\n", 5, FALSE);
						me->triggerStart();
						connected = TRUE;
						break;

					case CONNMGR_STATUS_DISCONNECTED:
						DBG_TRACE(L"Debug - Connection.cpp - Connection triggered [Out Action]\n", 5, FALSE);
						// Si potrebbe eseguire l'exit action qui
						me->triggerEnd();
						connected = FALSE;
						break;

					default: break;
				}
			}

			DispatchMessage(&msg);
		}

		waitDelay += GetTickCount() - now;

		if (connected && waitDelay >= delay && curIterations < iterations) {
			waitDelay = 0;
			me->triggerRepeat();
			curIterations++;
		}

		WaitForSingleObject(eventHandle, 5000);

		if (me->shouldStop()) {
			ConnMgrRegisterForStatusChangeNotification(FALSE, hWin);
			DestroyWindow(hWin);
			me->setStatus(EVENT_STOPPED);
			DBG_TRACE(L"Debug - Connection.cpp - Connection clean stop [0]\n", 5, FALSE);
			return 0;
		}
	}

	me->setStatus(EVENT_STOPPED);
	return 0;
}