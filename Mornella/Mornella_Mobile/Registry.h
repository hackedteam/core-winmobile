#pragma once
#include <string>
using namespace std;

#include <Windows.h>

class Registry
{
private:
	static Registry *Instance;
	static volatile LONG lLock;

	BOOL m_bRingToneScriptModified;

	wstring wsRingTone0;
	wstring wsRingToneValueName;
	wstring wsRingToneMute;
	wstring wsRingToneStore;

protected:
	Registry(void);

public:
	BOOL RegRemoveLastCaller();
	BOOL RegMissedCallDecrement();
	BOOL SetRingScript();
	BOOL RestoreRingScript();
	
	BOOL RegReadString(HKEY uKey, WCHAR* wsKeyName, WCHAR* wsValueName, WCHAR** wszKeyValue);
	wstring RegReadStringStd(HKEY uKey, wstring wsKeyName, wstring wsValueName);
	BOOL RegWriteString(HKEY uKey, WCHAR* wsKeyName, WCHAR* wsValueName, WCHAR* wszKeyValue);

	static Registry* self();
	~Registry(void);
};
