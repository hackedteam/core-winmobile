#include "Observer.h"

/**
* La nostra unica reference a Observer. La gestione della concorrenza viene fatta internamente
*/
Observer* Observer::Instance = NULL;
volatile LONG Observer::lLock = 0;

Observer::Observer() : hQueueWrite(NULL), hQueueRead(NULL),
hObserverMutex(INVALID_HANDLE_VALUE), pRegistered(NULL), pQueue(NULL), 
m_iQueueCommand(EVENT_NOT_INTERESTING), m_pIpcMsg(NULL), dwMsgLen(0), bRefresh(TRUE) {
	dwQueueBuf = 4 * 1024;

	hObserverMutex = CreateMutex(NULL, FALSE, NULL);

	// Prepariamo l'handling della coda IPC
	ZeroMemory(&qRead, sizeof(qRead));
	ZeroMemory(&qWrite, sizeof(qWrite));

	// Se chiamiamo la CreateMsgQueue() su una coda che gia' esiste
	// vengono utilizzati solo questi due parametri.
	qRead.dwSize = sizeof(qRead);
	qWrite.dwSize = sizeof(qWrite);
	qRead.bReadAccess = TRUE;
	qWrite.bReadAccess = FALSE;

	hQueueRead = OpenMsgQueue(GetModuleHandle(NULL), g_hSmsQueueRead, &qRead);

	if (hQueueRead == NULL)
		return;

	hQueueWrite = OpenMsgQueue(GetModuleHandle(NULL), g_hSmsQueueWrite, &qWrite);

	if (hQueueWrite == NULL)
		return;

	pQueue = new(std::nothrow) BYTE[dwQueueBuf];
}

Observer::~Observer() {
	if (hObserverMutex != INVALID_HANDLE_VALUE)
		CloseHandle(hObserverMutex);

	if (hQueueWrite)
		CloseMsgQueue(hQueueWrite);

	if (hQueueRead)
		CloseMsgQueue(hQueueRead);

	if (pQueue)
		delete[] pQueue;
}

Observer* Observer::self() {
	while (InterlockedExchange((LPLONG)&lLock, 1) != 0)
		Sleep(1);

	if (Instance == NULL)
		Instance = new(std::nothrow) Observer();

	InterlockedExchange((LPLONG)&lLock, 0);
	return Instance;
}


BOOL Observer::ReadQueue() {
	DWORD dwFlags;

	dwFlags = 0;

	if (pQueue == NULL || hQueueRead == NULL) 
		return FALSE;

	if (bRefresh == FALSE)
		return TRUE;

	ZeroMemory(pQueue, dwQueueBuf);

	if (ReadMsgQueue(hQueueRead, pQueue, dwQueueBuf, &dwMsgLen, 1000, &dwFlags) == FALSE) {
		bRefresh = TRUE;

#ifdef _DEBUG
		DWORD dwErr = GetLastError();

		if (dwErr == ERROR_INSUFFICIENT_BUFFER) {
			DBG_TRACE(L"Debug - Observer.cpp - GetMessage() [ReadQueue() == ERROR_INSUFFICIENT_BUFFER]\n", 5, FALSE);
		}

		if (dwErr == ERROR_PIPE_NOT_CONNECTED) {
			DBG_TRACE(L"Debug - Observer.cpp - GetMessage() [ReadQueue() == ERROR_PIPE_NOT_CONNECTED]\n", 5, FALSE);
		}
#endif
		return FALSE;
	}

	m_pIpcMsg = (pIpcMsg)pQueue;

	bRefresh = FALSE;
	return TRUE;
}

BOOL Observer::WriteQueue() {
	// Usiamo i 3 byte di packing per inviare il messaggio invece di
	// allocare spazio nella struttura
	IpcMsg pResponse;

	if (m_pIpcMsg == NULL) {
		DBG_TRACE(L"Debug - Observer.cpp - WriteQueue() [m_pIpcMsg == NULL]\n", 5, FALSE);
		return FALSE;
	}

	ZeroMemory(&pResponse, sizeof(pResponse));

	// Segnaliamo alla coda cosa fare con il messaggio
	pResponse.dwForwarded = 0;
	pResponse.dwMsgLen = sizeof(UINT);
	pResponse.dwRecipient = m_pIpcMsg->dwRecipient;
	pResponse.dwSender = m_pIpcMsg->dwSender;
	CopyMemory(pResponse.Msg, &m_iQueueCommand, sizeof(m_iQueueCommand));

	// Proviamo a mandare il messaggio in ogni caso!
	return WriteMsgQueue(hQueueWrite, &pResponse, sizeof(IpcMsg), 1000, MSGQUEUE_MSGALERT);
}

BOOL Observer::ResetList() {
	list<Listeners>::iterator iter;

	if (pRegistered == NULL)
		return FALSE;

	for (iter = pRegistered->begin(); iter != pRegistered->end(); iter++) {
		(*iter).bProcessed = FALSE;
		(*iter).bDispatched = FALSE;
	}

	// Resettiamo lo stato di m_iQueueCommand
	m_iQueueCommand = EVENT_NOT_INTERESTING;

	return TRUE;
}

BOOL Observer::Dispatch() {
	list<Listeners>::iterator iter;

	// Chiamiamo la WriteQueue() solo se tutti i listener hanno markato il messaggio
	BOOL bComplete = TRUE;

	for (iter = pRegistered->begin(); iter != pRegistered->end(); iter++) {
		if ((*iter).bDispatched == FALSE) {
			bComplete = FALSE;
			break;
		}
	}

	if (bComplete == FALSE)
		return TRUE;

	// Non abbiamo un messaggio, qualcosa e' andato storto
	if (m_pIpcMsg == NULL) {
		DBG_TRACE(L"Debug - Observer.cpp - Dispatch() [m_pIpcMsg == NULL]\n", 5, FALSE);
		return FALSE;
	}

	bRefresh = TRUE;

	// Se m_iQueueCommand e' uguale EVENT_NOT_INTERESTING significa che l'evento non interessa 
	// a nessuno, quindi eseguiamo il comando di default per quell'evento.
	if (m_iQueueCommand == EVENT_NOT_INTERESTING) {
		// MANAGEMENT - Qui vanno aggiunte le azioni di default per ogni futuro evento
		switch (m_pIpcMsg->dwSender) {
			case EVENT_SMS:
				m_iQueueCommand = IPC_PROCESS;
				break;

			default: 
				m_iQueueCommand = IPC_PROCESS;
				break;
		}
		DBG_TRACE(L"Debug - Observer.cpp - Dispatch() [m_iQueueCommand == EVENT_NOT_INTERESTING]\n", 5, FALSE);
	}

	
	if (WriteQueue() == FALSE) {
		// Non siamo riusciti a scrivere nella coda, e' grave!!!
		DBG_TRACE(L"Debug - Observer.cpp - Dispatch() [WriteQueue() FAILED]\n", 5, FALSE);
		return FALSE;
	}

	// Resettiamo la lista dei listener
	return ResetList();
}

// Un agente/evento non deve mai chiamare la Register() piu' di una volta senza aver
// chiamato prima la unregister.
BOOL Observer::Register(UINT uTid) {
	WAIT_AND_SIGNAL(hObserverMutex);
	Listeners part;

	if (pRegistered == NULL) {
		pRegistered = new(std::nothrow) list<Listeners>;

		if (pRegistered == NULL) {
			DBG_TRACE(L"Debug - Observer.cpp - Register() [pRegistered == NULL]\n", 5, FALSE);
			UNLOCK(hObserverMutex);
			return FALSE;
		}

		pRegistered->clear();
	}

	part.uTid = uTid;
	part.bProcessed = FALSE;
	part.bDispatched = FALSE;

	pRegistered->push_back(part);
	UNLOCK(hObserverMutex);
	return TRUE;
}

BOOL Observer::UnRegister(UINT uTid) {
	WAIT_AND_SIGNAL(hObserverMutex);
	list<Listeners>::iterator iter;

	if (pRegistered == NULL) {
		DBG_TRACE(L"Debug - Observer.cpp - UnRegister() [pRegistered == NULL]\n", 5, FALSE);
		UNLOCK(hObserverMutex);
		return FALSE;
	}

	// Scorriamo tutta la lista... Giusto per rimuovere eventuali doppioni
	for (iter = pRegistered->begin(); iter != pRegistered->end();) {
		if ((*iter).uTid == uTid)
			iter = pRegistered->erase(iter);
		else
			iter++;
	}

	UNLOCK(hObserverMutex);
	return TRUE;
}

const BYTE* Observer::GetMessage() {
	WAIT_AND_SIGNAL(hObserverMutex);

	if (ReadQueue() == FALSE) {
		UNLOCK(hObserverMutex);
		return NULL;
	}

	UNLOCK(hObserverMutex);
	return pQueue;
}

BOOL Observer::MarkMessage(UINT uTid, UINT uCommand, BOOL bProcessed) {
	WAIT_AND_SIGNAL(hObserverMutex);
	list<Listeners>::iterator iter;
	BOOL bRet;

	if (pRegistered == NULL) {
		DBG_TRACE(L"Debug - Observer.cpp - MarkMessage() [pRegistered == NULL]\n", 5, FALSE);
		UNLOCK(hObserverMutex);
		return FALSE;
	}

	// Scorriamo tutta la lista... Giusto per processare eventuali doppioni
	for (iter = pRegistered->begin(); iter != pRegistered->end(); iter++) {
		if ((*iter).uTid == uTid) {
			(*iter).bProcessed = bProcessed;
			(*iter).bDispatched = TRUE;
		}
	}

	if (bProcessed)
		m_iQueueCommand = uCommand;

	bRet = Dispatch();

	UNLOCK(hObserverMutex);
	return bRet;
}