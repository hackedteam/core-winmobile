#pragma once

class CPoomContact;
class CPoomCalendar;
class CPoomTask;

class CPoomSerializer
{
private:
	DWORD _SerializedStringLength(LPWSTR lpString);
	DWORD _SerializeString(LPBYTE lpDest, LPTSTR lpString, int entryType);
	DWORD _Prefix(DWORD dwLength, int entryType);

public:
	CPoomSerializer(){}
	virtual ~CPoomSerializer(void){}

	LPBYTE Serialize(CPoomContact *contact, LPDWORD lpdwOutLength);
	LPBYTE Serialize(CPoomCalendar *calendar, LPDWORD lpdwOutLength);
	LPBYTE Serialize(CPoomTask *task, LPDWORD lpdwOutLength);
};