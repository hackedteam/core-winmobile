// Classe per la gestione dello stack bluetooth di Windows Mobile
#pragma once

#include "BlueTooth.h"
#include "Device.h"
#include "Status.h"

class WindowsBlueTooth : public BlueTooth 
{
private:
	BOOL m_bIsBtAlreadyActive;
	GUID m_btGuid;
	WCHAR m_wInstanceName[33];
	Device *deviceObj;
	Status *statusObj;

public:
	WindowsBlueTooth(GUID bt);
	~WindowsBlueTooth();

	BOOL InquiryService(WCHAR *address);
	BT_ADDR FindServer(SOCKET bt);
	BOOL ActivateBT();
	BOOL DeActivateBT();
	BOOL SetInstanceName(PWCHAR pwName);
	void SetGuid(GUID guid);
};