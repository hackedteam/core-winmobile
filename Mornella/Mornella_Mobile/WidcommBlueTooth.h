// Classe per la gestione dello stack bluetooth di WidComm
#pragma once

#include "BlueTooth.h"
#include "Device.h"
#include "Status.h"

// Facciamo l'undef di questa macro perche' anche Windows la 
// definisce per i fatti suoi, ma quella di Widcomm ha un altro valore.
#undef L2CAP_MAX_MTU
//#include <BtSdkCE.h>

typedef enum 
{
	CONNECT_ALLOW_NONE,
	CONNECT_ALLOW_ALL, 
	CONNECT_ALLOW_PAIRED, 
	CONNECT_ALLOW_FILTERED
} CONNECT_ALLOW_TYPE;

typedef HRESULT t_OEMSetBthPowerState(DWORD);

// Widcomm
typedef PVOID t_CBtIfConstructor(void);
typedef PVOID t_CBtIfDestructor(PVOID);
typedef BOOL t_IsStackServerUp(PVOID);
typedef void t_AllowToConnect(PVOID, CONNECT_ALLOW_TYPE ConnectType);
typedef BOOL t_SetDiscoverable(PVOID, BOOL bDiscoverable);

class WidcommBlueTooth : public BlueTooth 
{
private:
	BOOL m_bIsBtAlreadyActive;
	GUID m_btGuid;
	WCHAR m_wInstanceName[33];
	HMODULE m_hBT, m_hWidcomm;
	HANDLE m_hMutex;
	Device *deviceObj;
	Status *statusObj;

private:
	// Funzioni per la gestione del BT
	t_OEMSetBthPowerState *m_OEMSetBthPowerState;

	// Funzioni dello stack Widcomm
	t_CBtIfConstructor *m_CBtIfConstructor;
	t_CBtIfDestructor *m_CBtIfDestructor;
	t_IsStackServerUp *m_IsStackServerUp;
	t_AllowToConnect *m_AllowToConnect;
	t_SetDiscoverable *m_SetDiscoverable;

private:
	// Torna TRUE se il BT e' gia' attivo, FALSE altrimenti
	BOOL IsBTActive();

	// Overload del metodo per la ricezione delle informazioni sui device trovati
	void OnDeviceResponded(BD_ADDR p_bda, DEV_CLASS dev_class, BD_NAME bd_name, BOOL bConnected);

public:
	WidcommBlueTooth(GUID bt);
	~WidcommBlueTooth();

	BOOL InquiryService(WCHAR *address);
	BT_ADDR FindServer(SOCKET bt);
	BOOL ActivateBT();
	BOOL DeActivateBT();
	BOOL SetInstanceName(PWCHAR pwName);
	void SetGuid(GUID guid);
};