#pragma once

#include "PoomMan.h"

class CPoomCalendarReader :
	public IPoomFolderReader
{
private: 
	void Parse(IAppointment *appointment, CPoomCalendar *calendar);
	IPOutlookItemCollection* _items;

public:
	CPoomCalendarReader(IFolder* pIFolder);
	~CPoomCalendarReader(void);
	
	int Count();
	
	HRESULT Get(int i, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT *uBufLen);
	HRESULT GetOne(IPOutlookApp* _pIPoomApp, LONG lOid, CPoomSerializer *pPoomSerializer, LPBYTE *pBuf, UINT* puBufLen);
};